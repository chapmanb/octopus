// Copyright (c) 2016 Daniel Cooke
// Use of this source code is governed by the MIT license that can be found in the LICENSE file.

#include "trio_caller.hpp"

#include <typeinfo>
#include <functional>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <map>

#include "basics/genomic_region.hpp"
#include "core/types/allele.hpp"
#include "core/types/variant.hpp"
#include "containers/probability_matrix.hpp"
#include "utils/germline_variant_call.hpp"
#include "utils/reference_call.hpp"
#include "utils/map_utils.hpp"

namespace octopus {

TrioCaller::TrioCaller(Caller::Components&& components,
                       Caller::Parameters general_parameters,
                       Parameters specific_parameters)
: Caller {std::move(components), std::move(general_parameters)}
, parameters_ {std::move(specific_parameters)}
{
    if (parameters_.maternal_ploidy == 0) {
        throw std::logic_error {"IndividualCaller: ploidy must be > 0"};
    }
}

Caller::CallTypeSet TrioCaller::do_get_call_types() const
{
    return {std::type_index(typeid(GermlineVariantCall))};
}

namespace {

using model::TrioModel;
using JointProbability = TrioModel::Latents::JointProbability;

using GenotypeReference = std::reference_wrapper<const Genotype<Haplotype>>;

struct GenotypeReferenceLess
{
    bool operator()(GenotypeReference lhs, GenotypeReference rhs) const
    {
        return GenotypeLess{}(lhs.get(), rhs.get());
    }
};
struct GenotypeReferenceEqual
{
    bool operator()(GenotypeReference lhs, GenotypeReference rhs) const
    {
        return lhs.get() == rhs.get();
    }
};

struct GenotypeProbabilityPair
{
    GenotypeReference genotype;
    double probability;
};

template <typename Function>
auto marginalise(std::vector<JointProbability>& joint_posteriors, Function who)
{
    std::sort(std::begin(joint_posteriors), std::end(joint_posteriors),
              [who] (const auto& lhs, const auto& rhs) {
                  return GenotypeReferenceLess{}(who(lhs), who(rhs));
              });
    std::vector<GenotypeProbabilityPair> result {};
    result.reserve(joint_posteriors.size());
    for (auto iter = std::cbegin(joint_posteriors), end = std::cend(joint_posteriors); iter != end;) {
        const auto next = std::find_if_not(std::next(iter), end,
                                           [iter, who] (const auto& p) {
                                               return GenotypeReferenceEqual{}(who(p), who(*iter));
                                           });
        result.push_back({who(*iter),
                          std::accumulate(iter, next, 0.0,
                                          [] (const auto curr, const auto& p) {
                                              return curr + p.probability;
                                          })});
        iter = next;
    }
    return result;
}

auto marginalise_mother(std::vector<JointProbability>& joint_posteriors)
{
    return marginalise(joint_posteriors, [] (const JointProbability& p) -> GenotypeReference { return p.maternal; });
}

auto marginalise_father(std::vector<JointProbability>& joint_posteriors)
{
    return marginalise(joint_posteriors, [] (const JointProbability& p) -> GenotypeReference { return p.paternal; });
}

auto marginalise_child(std::vector<JointProbability>& joint_posteriors)
{
    return marginalise(joint_posteriors, [] (const JointProbability& p) -> GenotypeReference { return p.child; });
}

struct GenotypePairLess
{
    bool operator()(const GenotypeReference lhs, const GenotypeProbabilityPair& rhs) const
    {
        return GenotypeReferenceLess{}(lhs, rhs.genotype);
    }
    bool operator()(const GenotypeProbabilityPair& lhs, const GenotypeReference rhs) const
    {
        return GenotypeReferenceLess{}(lhs.genotype, rhs);
    }
};

// genotypes is required to be sorted
void fill_missing_genotypes(std::vector<GenotypeProbabilityPair>& posteriors,
                            const std::vector<Genotype<Haplotype>>& genotypes)
{
    std::sort(std::begin(posteriors), std::end(posteriors),
              [] (const auto& lhs, const auto& rhs) {
                  return GenotypeReferenceLess{}(lhs.genotype, rhs.genotype);
              });
    std::vector<GenotypeReference> missing {};
    missing.reserve(genotypes.size());
    std::set_difference(std::cbegin(genotypes), std::cend(genotypes),
                        std::cbegin(posteriors), std::cend(posteriors),
                        std::back_inserter(missing), GenotypePairLess {});
    std::transform(std::cbegin(missing), std::cend(missing),
                   std::back_inserter(posteriors),
                   [] (auto genotype) -> GenotypeProbabilityPair {
                       return {genotype, 0.0};
                   });
    posteriors.shrink_to_fit();
}

auto sort_copy(std::vector<Genotype<Haplotype>> genotypes)
{
    std::sort(std::begin(genotypes), std::end(genotypes), GenotypeLess {});
    return genotypes;
}

} // namespace

TrioCaller::Latents::Latents(const std::vector<Haplotype>& haplotypes,
                             std::vector<Genotype<Haplotype>>&& genotypes,
                             model::TrioModel::InferredLatents&& latents,
                             const Trio& trio)
: maternal {std::move(genotypes)}
, latents {std::move(latents)}
{
    auto maternal_posteriors = marginalise_mother(latents.posteriors.joint_genotype_probabilities);
    auto paternal_posteriors = marginalise_father(latents.posteriors.joint_genotype_probabilities);
    auto child_posteriors = marginalise_child(latents.posteriors.joint_genotype_probabilities);
    const auto sorted_genotypes = sort_copy(maternal);
    fill_missing_genotypes(maternal_posteriors, sorted_genotypes);
    fill_missing_genotypes(paternal_posteriors, sorted_genotypes);
    fill_missing_genotypes(child_posteriors, sorted_genotypes);
    
    // TODO
    std::vector<double> child_flat_posteriors(child_posteriors.size());
    std::transform(std::cbegin(child_posteriors), std::cend(child_posteriors),
                   std::begin(child_flat_posteriors),
                   [] (const auto& p) { return p.probability; });
    GenotypeProbabilityMap genotype_posteriors {std::begin(maternal), std::end(maternal)};
    insert_sample(trio.child(), child_flat_posteriors, genotype_posteriors);
    insert_sample(trio.mother(), child_flat_posteriors, genotype_posteriors);
    insert_sample(trio.father(), child_flat_posteriors, genotype_posteriors);
    marginal_genotype_posteriors = std::make_shared<GenotypeProbabilityMap>(std::move(genotype_posteriors));
    
    HaplotypeProbabilityMap haplotype_posteriors {haplotypes.size()};
    for (const auto& haplotype : haplotypes) {
        haplotype_posteriors.emplace(haplotype, 1.0 / haplotypes.size());
    }
    marginal_haplotype_posteriors = std::make_shared<HaplotypeProbabilityMap>(haplotype_posteriors);
}

std::shared_ptr<TrioCaller::Latents::HaplotypeProbabilityMap>
TrioCaller::Latents::haplotype_posteriors() const noexcept
{
    return marginal_haplotype_posteriors;
}

std::shared_ptr<TrioCaller::Latents::GenotypeProbabilityMap>
TrioCaller::Latents::genotype_posteriors() const noexcept
{
    return marginal_genotype_posteriors;
}

std::unique_ptr<Caller::Latents>
TrioCaller::infer_latents(const std::vector<Haplotype>& haplotypes,
                          const HaplotypeLikelihoodCache& haplotype_likelihoods) const
{
    const CoalescentModel germline_prior_model {
        Haplotype {mapped_region(haplotypes.front()), reference_},
        parameters_.germline_prior_model_params
    };
    const DeNovoModel denovo_model {parameters_.denovo_model_params};
    const model::TrioModel model {parameters_.trio, germline_prior_model, denovo_model};
    auto genotypes = generate_all_genotypes(haplotypes, parameters_.maternal_ploidy);
    auto latents = model.evaluate(genotypes, genotypes, genotypes, haplotype_likelihoods);
    return std::make_unique<Latents>(haplotypes, std::move(genotypes), std::move(latents), parameters_.trio);
}

boost::optional<double>
TrioCaller::calculate_model_posterior(const std::vector<Haplotype>& haplotypes,
                                      const HaplotypeLikelihoodCache& haplotype_likelihoods,
                                      const Caller::Latents& latents) const
{
    return calculate_model_posterior(haplotypes, haplotype_likelihoods, dynamic_cast<const Latents&>(latents));
}

boost::optional<double>
TrioCaller::calculate_model_posterior(const std::vector<Haplotype>& haplotypes,
                                      const HaplotypeLikelihoodCache& haplotype_likelihoods,
                                      const Latents& latents) const
{
    return boost::none;
}

std::vector<std::unique_ptr<VariantCall>>
TrioCaller::call_variants(const std::vector<Variant>& candidates, const Caller::Latents& latents) const
{
    return call_variants(candidates, dynamic_cast<const Latents&>(latents));
}

namespace {

using JointProbability      = TrioModel::Latents::JointProbability;
using TrioProbabilityVector = std::vector<JointProbability>;

bool contains(const JointProbability& trio, const Allele& allele)
{
    return contains(trio.maternal, allele)
           || contains(trio.paternal, allele)
           || contains(trio.child, allele);
}

auto compute_posterior(const Allele& allele, const TrioProbabilityVector& trio_posteriors)
{
    auto p = std::accumulate(std::cbegin(trio_posteriors), std::cend(trio_posteriors),
                             0.0, [&allele] (const auto curr, const auto& p) {
        return curr + (contains(p, allele) ? 0.0 : p.probability);
    });
    return probability_to_phred(p);
}

using AllelePosteriorMap = std::map<Allele, Phred<double>>;

auto compute_posteriors(const std::vector<Allele>& alleles, const TrioProbabilityVector& trio_posteriors)
{
    AllelePosteriorMap result {};
    for (const auto& allele : alleles) {
        result.emplace(allele, compute_posterior(allele, trio_posteriors));
    }
    return result;
}

struct TrioCall
{
    Genotype<Haplotype> mother, father, child;
};

auto call_trio(const TrioProbabilityVector& trio_posteriors)
{
    auto iter = std::max_element(std::cbegin(trio_posteriors), std::cbegin(trio_posteriors),
                            [] (const auto& lhs, const auto& rhs) {
                                return lhs.probability > rhs.probability;
                            });
    return TrioCall {iter->maternal, iter->paternal, iter->child};
}

bool includes(const TrioCall& trio, const Allele& allele)
{
    return includes(trio.mother, allele)
           || includes(trio.father, allele)
           || includes(trio.child, allele);
}

auto call_alleles(const AllelePosteriorMap& allele_posteriors,
                  const TrioCall& called_trio,
                  const Phred<double> min_posterior)
{
    AllelePosteriorMap result {};
    
    std::copy_if(std::cbegin(allele_posteriors), std::cend(allele_posteriors),
                 std::inserter(result, std::begin(result)),
                 [&called_trio, min_posterior] (const auto& p) {
                     return p.second >= min_posterior && includes(called_trio, p.first);
                 });
    
    return result;
}

bool is_denovo(const Allele& allele, const JointProbability& trio)
{
    return contains(trio.child, allele) && !(contains(trio.maternal, allele) || contains(trio.paternal, allele));
}

auto compute_denovo_posterior(const Allele& allele, const TrioProbabilityVector& trio_posteriors)
{
    auto p = std::accumulate(std::cbegin(trio_posteriors), std::cend(trio_posteriors),
                             0.0, [&allele] (const auto curr, const auto& p) {
        return curr + (is_denovo(allele, p) ? 0.0 : p.probability);
    });
    return probability_to_phred(p);
}

auto compute_denovo_posteriors(const AllelePosteriorMap& called_alleles,
                               const TrioProbabilityVector& trio_posteriors)
{
    AllelePosteriorMap result {};
    
    for (const auto& p : called_alleles) {
        result.emplace(p.first, compute_denovo_posterior(p.first, trio_posteriors));
    }
    
    return result;
}

auto call_denovos(const AllelePosteriorMap& denovo_posteriors,
                  const Genotype<Haplotype>& called_child,
                  const Phred<double> min_posterior)
{
    AllelePosteriorMap result {};
    
    std::copy_if(std::cbegin(denovo_posteriors), std::cend(denovo_posteriors),
                 std::inserter(result, std::begin(result)),
                 [&called_child, min_posterior] (const auto& p) {
                     return p.second >= min_posterior && includes(called_child, p.first);
                 });
    
    return result;
}

auto extract_regions(const AllelePosteriorMap& calls)
{
    auto result = extract_regions(extract_keys(calls));
    std::sort(std::begin(result), std::end(result));
    result.erase(std::unique(std::begin(result), std::end(result)), std::end(result));
    return result;
}

using GenotypeProbabilityMap = ProbabilityMatrix<Genotype<Haplotype>>;

auto compute_posterior(const Genotype<Allele>& genotype,
                       const GenotypeProbabilityMap::InnerMap& posteriors)
{
    auto p = std::accumulate(std::cbegin(posteriors), std::cend(posteriors), 0.0,
                             [&genotype] (const double curr, const auto& p) {
                                 return curr + (contains(p.first, genotype) ? 0.0 : p.second);
                             });
    return probability_to_phred(p);
}

auto call_genotypes(const Trio& trio, const TrioCall& called_trio,
                    const GenotypeProbabilityMap& trio_posteriors,
                    const std::vector<GenomicRegion>& regions)
{
    std::vector<Genotype<Allele>> result {};
    result.reserve(regions.size());
    
    for (const auto& region : regions) {
        auto genotype = splice<Allele>(called_trio.child, region);
        auto p = compute_posterior(genotype, trio_posteriors[trio.child()]);
        result.emplace_back(std::move(genotype));
    }
    
    return result;
}

} // namespace

std::vector<std::unique_ptr<VariantCall>>
TrioCaller::call_variants(const std::vector<Variant>& candidates, const Latents& latents) const
{
    const auto alleles = decompose(candidates);
    const auto& trio_posteriors = latents.latents.posteriors.joint_genotype_probabilities;
    const auto allele_posteriors = compute_posteriors(alleles, trio_posteriors);
    const auto called_trio = call_trio(trio_posteriors);
    const auto called_alleles = call_alleles(allele_posteriors, called_trio, parameters_.min_variant_posterior);
    const auto denovo_posteriors = compute_denovo_posteriors(called_alleles, trio_posteriors);
    const auto called_denovos = call_denovos(denovo_posteriors, called_trio.child, parameters_.min_variant_posterior);
    const auto denovo_regions = extract_regions(called_denovos);
    if (!called_denovos.empty()) {
        std::cout << "De Novo!" << std::endl;
        for (const auto& p : called_denovos) {
            std::cout << p.first << " " << p.second << std::endl;
        }
    }
    return {};
}

std::vector<std::unique_ptr<ReferenceCall>>
TrioCaller::call_reference(const std::vector<Allele>& alleles, const Caller::Latents& latents,
                           const ReadMap& reads) const
{
    return call_reference(alleles, dynamic_cast<const Latents&>(latents), reads);
}

std::vector<std::unique_ptr<ReferenceCall>>
TrioCaller::call_reference(const std::vector<Allele>& alleles, const Latents& latents,
                            const ReadMap& reads) const
{
    return {};
}

} // namespace octopus
