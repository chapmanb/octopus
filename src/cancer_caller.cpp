//
//  cancer_caller.cpp
//  Octopus
//
//  Created by Daniel Cooke on 16/09/2015.
//  Copyright (c) 2015 Oxford University. All rights reserved.
//

#include "cancer_caller.hpp"

#include <typeinfo>
#include <string>
#include <utility>
#include <algorithm>
#include <numeric>
#include <deque>
#include <unordered_set>
#include <stdexcept>
#include <iostream>

#include <boost/iterator/zip_iterator.hpp>
#include <boost/tuple/tuple.hpp>

#include "genomic_region.hpp"
#include "read_pipe.hpp"
#include "allele.hpp"
#include "variant.hpp"
#include "haplotype.hpp"
#include "genotype.hpp"
#include "merge_transform.hpp"
#include "mappable_algorithms.hpp"
#include "maths.hpp"
#include "cancer_genotype.hpp"
#include "read_stats.hpp"
#include "probability_matrix.hpp"
#include "sequence_utils.hpp"
#include "germline_variant_call.hpp"
#include "reference_call.hpp"
#include "somatic_call.hpp"
#include "logging.hpp"

#include "timers.hpp"

namespace octopus {
// public methods

CancerCaller::CancerCaller(Caller::Components&& components,
                           Caller::Parameters general_parameters,
                           Parameters specific_parameters)
:
Caller {std::move(components), std::move(general_parameters)},
parameters_ {std::move(specific_parameters)}
{
    if (parameters_.ploidy == 0) {
        throw std::logic_error {"CancerCaller: ploidy must be > 0"};
    }
    
    if (parameters_.max_genotypes == 0) {
        throw std::logic_error {"CancerCaller: max genotypes must be > 0"};
    }
    
    if (std::find(std::cbegin(samples_), std::cend(samples_), parameters_.normal_sample) == std::cend(samples_)) {
        throw std::invalid_argument {"CancerCaller: normal sample is not a valid sample"};
    }
    
    if (parameters_.min_variant_posterior == Phred<double> {0}) {
        logging::WarningLogger wlog {};
        wlog << "Having no germline variant posterior threshold means no somatic variants will be called";
    }
    
    if (debug_log_) {
        if (has_normal_sample()) {
            stream(*debug_log_) << "Normal sample is " << *parameters_.normal_sample;
        } else {
            *debug_log_ << "There is no normal sample";
        }
    }
}

CancerCaller::CallTypeSet CancerCaller::do_get_call_types() const
{
    return {
        std::type_index(typeid(GermlineVariantCall)),
        std::type_index(typeid(SomaticCall))
    };
}

CancerCaller::Latents::Latents(const std::vector<Haplotype>& haplotypes,
                               CancerCaller::ModelPriors model_priors,
                               std::vector<Genotype<Haplotype>>&& germline_genotypes,
                               std::vector<CancerGenotype<Haplotype>>&& somatic_genotypes,
                               GermlineModel::InferredLatents&& germline,
                               CNVModel::InferredLatents&& cnv,
                               TumourModel::InferredLatents&& somatic,
                               const std::vector<SampleName>& samples,
                               boost::optional<std::reference_wrapper<const SampleName>> normal_sample)
:
germline_genotypes_ {std::move(germline_genotypes)},
somatic_genotypes_ {std::move(somatic_genotypes)},
model_priors_ {model_priors},
germline_model_inferences_ {std::move(germline)},
cnv_model_inferences_ {std::move(cnv)},
somatic_model_inferences_ {std::move(somatic)},
haplotypes_ {haplotypes},
samples_ {samples},
normal_sample_ {normal_sample}
{}
    
template <typename... T>
auto zip(const T&... containers)
        -> boost::iterator_range<boost::zip_iterator<decltype(boost::make_tuple(std::begin(containers)...))>>
{
    auto zip_begin = boost::make_zip_iterator(boost::make_tuple(std::begin(containers)...));
    auto zip_end   = boost::make_zip_iterator(boost::make_tuple(std::end(containers)...));
    return boost::make_iterator_range(zip_begin, zip_end);
}

std::shared_ptr<CancerCaller::Latents::HaplotypeProbabilityMap>
CancerCaller::Latents::haplotype_posteriors() const
{
    Latents::HaplotypeProbabilityMap result {haplotypes_.get().size()};
    
    for (const auto& haplotype : haplotypes_.get()) {
        result.emplace(haplotype, 0.0);
    }
    
    for (const auto& p :zip(germline_genotypes_, germline_model_inferences_.posteriors.genotype_probabilities)) {
        for (const auto& haplotype : p.get<0>().copy_unique_ref()) {
            result.at(haplotype) += p.get<1>();
        }
    }
    
    Latents::HaplotypeProbabilityMap cnv_result {haplotypes_.get().size()};
    
    for (const auto& haplotype : haplotypes_.get()) {
        cnv_result.emplace(haplotype, 0.0);
    }
    
    for (const auto& p : cnv_model_inferences_.posteriors.genotype_probabilities) {
        for (const auto& haplotype : p.first.copy_unique_ref()) {
            cnv_result.at(haplotype) += p.second;
        }
    }
    
    Latents::HaplotypeProbabilityMap somatic_result {haplotypes_.get().size()};
    
    for (const auto& haplotype : haplotypes_.get()) {
        somatic_result.emplace(haplotype, 0.0);
    }
    
    for (const auto& p : somatic_model_inferences_.posteriors.genotype_probabilities) {
        for (const auto& haplotype : p.first.germline_genotype().copy_unique_ref()) {
            somatic_result.at(haplotype) += p.second;
        }
        somatic_result.at(p.first.somatic_element()) += p.second;
    }
    
    for (auto& p : result) {
        p.second *= model_priors_.germline;
        p.second += model_priors_.cnv * cnv_result.at(p.first);
        p.second += model_priors_.somatic * somatic_result.at(p.first);
    }
    
    return std::make_shared<Latents::HaplotypeProbabilityMap>(std::move(result));
}

std::shared_ptr<CancerCaller::Latents::GenotypeProbabilityMap>
CancerCaller::Latents::genotype_posteriors() const
{
    // TODO: properly
    
    GenotypeProbabilityMap genotype_posteriors {
        std::begin(germline_genotypes_), std::end(germline_genotypes_)
    };
    
    for (const auto& sample : samples_.get()) {
        insert_sample(std::move(sample), germline_model_inferences_.posteriors.genotype_probabilities,
                      genotype_posteriors);
    }
    
    return std::make_shared<Latents::GenotypeProbabilityMap>(std::move(genotype_posteriors));
}

// private methods

bool CancerCaller::has_normal_sample() const noexcept
{
    return static_cast<bool>(parameters_.normal_sample);
}

const SampleName& CancerCaller::normal_sample() const
{
    return *parameters_.normal_sample;
}

std::unique_ptr<CancerCaller::Caller::Latents>
CancerCaller::infer_latents(const std::vector<Haplotype>& haplotypes,
                            const HaplotypeLikelihoodCache& haplotype_likelihoods) const
{
    const auto ploidy = parameters_.ploidy;
    
    std::vector<CancerGenotype<Haplotype>> cancer_genotypes;
    std::vector<Genotype<Haplotype>> germline_genotypes;
    
    std::tie(cancer_genotypes, germline_genotypes) = generate_all_cancer_genotypes(haplotypes, ploidy);
    
    if (debug_log_) {
        stream(*debug_log_) << "There are " << germline_genotypes.size() << " candidate germline genotypes";
        stream(*debug_log_) << "There are " << cancer_genotypes.size() << " candidate cancer genotypes";
    }
    
    const CoalescentModel germline_prior_model {Haplotype {octopus::mapped_region(haplotypes.front()), reference_}};
    const SomaticMutationModel somatic_prior_model {germline_prior_model, parameters_.somatic_mutation_rate};
    
    auto cnv_model_priors     = get_cnv_model_priors(germline_prior_model);
    auto somatic_model_priors = get_somatic_model_priors(somatic_prior_model);
    
    const GermlineModel germline_model {germline_prior_model};
    const CNVModel cnv_model {samples_, ploidy, std::move(cnv_model_priors)};
    const TumourModel somatic_model {samples_, ploidy, std::move(somatic_model_priors)};
    
    static const SampleName merged_sample {"merged"};
    
    const auto merged_likelihoods = merge_samples(samples_, merged_sample, haplotypes,
                                                  haplotype_likelihoods);
    
    merged_likelihoods.prime(merged_sample);
    
    auto germline_inferences = germline_model.infer_latents(germline_genotypes, merged_likelihoods);
    
    auto cnv_inferences = cnv_model.infer_latents(germline_genotypes, haplotype_likelihoods);
    
    filter(cancer_genotypes, germline_genotypes, germline_inferences, cnv_inferences);
    
    auto somatic_inferences = somatic_model.infer_latents(cancer_genotypes, haplotype_likelihoods);
    
    boost::optional<std::reference_wrapper<const SampleName>> normal {};
    
    if (has_normal_sample()) normal = std::cref(normal_sample());
    
    return std::make_unique<Latents>(haplotypes,
                                     get_model_priors(),
                                     std::move(germline_genotypes), std::move(cancer_genotypes),
                                     std::move(germline_inferences), std::move(cnv_inferences),
                                     std::move(somatic_inferences), std::cref(samples_),
                                     normal);
}

auto extract_low_posterior_genotypes(const std::vector<Genotype<Haplotype>>& genotypes,
                                     const model::IndividualModel::Latents& latents,
                                     const double min_posterior = 1e-30)
{
    using GenotypeReference = std::reference_wrapper<const Genotype<Haplotype>>;
    
    auto hash = std::hash<GenotypeReference>();
    
    const auto cmp = [] (const auto& lhs, const auto& rhs) { return lhs.get() == rhs.get(); };
    
    std::unordered_set<GenotypeReference, decltype(hash), decltype(cmp)> result {
        genotypes.size(), hash, cmp
    };
    
    for (const auto& p : zip(genotypes, latents.genotype_probabilities)) {
        if (p.get<1>() < min_posterior) {
            result.emplace(p.get<0>());
        }
    }
    
    return result;
}

void CancerCaller::filter(std::vector<CancerGenotype<Haplotype>>& cancer_genotypes,
                          const std::vector<Genotype<Haplotype>>& germline_genotypes,
                          const GermlineModel::InferredLatents& germline_inferences,
                          const CNVModel::InferredLatents& cnv_inferences) const
{
    if (cancer_genotypes.size() <= parameters_.max_genotypes) return;
    
    if (has_normal_sample()) {
        const auto removable_germlines = extract_low_posterior_genotypes(germline_genotypes,
                                                                         germline_inferences.posteriors);
        
        const auto it = std::remove_if(std::begin(cancer_genotypes), std::end(cancer_genotypes),
                                       [&removable_germlines] (const auto& g) {
                                           return removable_germlines.count(g.germline_genotype()) == 1;
                                       });
        
        cancer_genotypes.erase(it, std::end(cancer_genotypes));
        
        if (cancer_genotypes.capacity() > 2 * cancer_genotypes.size()) {
            cancer_genotypes.shrink_to_fit();
        }
    } else {
        // TODO
    }
}

boost::optional<double>
CancerCaller::calculate_model_posterior(const std::vector<Haplotype>& haplotypes,
                                        const HaplotypeLikelihoodCache& haplotype_likelihoods,
                                        const Caller::Latents& latents) const
{
    return calculate_model_posterior(haplotypes, haplotype_likelihoods,
                                     dynamic_cast<const Latents&>(latents));
}

static double calculate_model_posterior(const double normal_germline_model_log_evidence,
                                        const double normal_dummy_model_log_evidence)
{
    constexpr double normal_model_prior {0.999};
    constexpr double dummy_model_prior {1.0 - normal_model_prior};
    
    const auto normal_model_ljp = std::log(normal_model_prior) + normal_germline_model_log_evidence;
    const auto dummy_model_ljp  = std::log(dummy_model_prior) + normal_dummy_model_log_evidence;
    
    const auto norm = maths::log_sum_exp(normal_model_ljp, dummy_model_ljp);
    
    return std::exp(normal_model_ljp - norm);
}

boost::optional<double>
CancerCaller::calculate_model_posterior(const std::vector<Haplotype>& haplotypes,
                                        const HaplotypeLikelihoodCache& haplotype_likelihoods,
                                        const Latents& latents) const
{
    if (has_normal_sample()) {
        const CoalescentModel prior_model {Haplotype {octopus::mapped_region(haplotypes.front()), reference_}};
        
        const GermlineModel germline_model {prior_model};
        
        haplotype_likelihoods.prime(normal_sample());
        
        const auto normal_inferences = germline_model.infer_latents(latents.germline_genotypes_,
                                                                    haplotype_likelihoods);
        
        const auto dummy_genotypes = generate_all_genotypes(haplotypes, parameters_.ploidy + 1);
        
        const auto dummy_inferences = germline_model.infer_latents(dummy_genotypes,
                                                                   haplotype_likelihoods);
        
        return octopus::calculate_model_posterior(normal_inferences.log_evidence,
                                                  dummy_inferences.log_evidence);
    }
    
    // TODO
    
    return boost::none;
}

CancerCaller::CNVModel::Priors
CancerCaller::get_cnv_model_priors(const CoalescentModel& prior_model) const
{
    using Priors = CNVModel::Priors;
    
    Priors::GenotypeMixturesDirichletAlphaMap cnv_alphas {};
    cnv_alphas.reserve(samples_.size());
    
    for (const auto& sample : samples_) {
        if (has_normal_sample() && sample == normal_sample()) {
            Priors::GenotypeMixturesDirichletAlphas sample_alphas {10.0, 10.0};
            cnv_alphas.emplace(sample, std::move(sample_alphas));
        } else {
            Priors::GenotypeMixturesDirichletAlphas sample_alphas {0.75, 0.75};
            cnv_alphas.emplace(sample, std::move(sample_alphas));
        }
    }
    
    return Priors {prior_model, std::move(cnv_alphas)};
}

CancerCaller::TumourModel::Priors
CancerCaller::get_somatic_model_priors(const SomaticMutationModel& prior_model) const
{
    using Priors = TumourModel::Priors;
    
    Priors::GenotypeMixturesDirichletAlphaMap alphas {};
    alphas.reserve(samples_.size());
    
    for (const auto& sample : samples_) {
        if (has_normal_sample() && sample == normal_sample()) {
            Priors::GenotypeMixturesDirichletAlphas sample_alphas {10.0, 10.0, 0.01};
            alphas.emplace(sample, std::move(sample_alphas));
        } else {
            Priors::GenotypeMixturesDirichletAlphas sample_alphas {1.0, 1.0, 0.8};
            alphas.emplace(sample, std::move(sample_alphas));
        }
    }
    
    return Priors {prior_model, std::move(alphas)};
}

std::vector<std::unique_ptr<VariantCall>>
CancerCaller::call_variants(const std::vector<Variant>& candidates,
                                   const Caller::Latents& latents) const
{
    return call_variants(candidates, dynamic_cast<const Latents&>(latents));
}

namespace
{
using VariantReference  = std::reference_wrapper<const Variant>;

using VariantPosteriors = std::vector<std::pair<VariantReference, Phred<double>>>;

struct GermlineVariantCall : Mappable<GermlineVariantCall>
{
    GermlineVariantCall() = delete;
    GermlineVariantCall(const std::pair<VariantReference, Phred<double>>& p)
    : variant {p.first}, posterior {p.second} {}
    GermlineVariantCall(const Variant& variant, Phred<double> posterior)
    : variant {variant}, posterior {posterior} {}
    
    const GenomicRegion& mapped_region() const noexcept { return octopus::mapped_region(variant.get()); }
    
    VariantReference variant;
    Phred<double> posterior;
};

using GermlineVariantCalls = std::vector<GermlineVariantCall>;

struct SomaticVariantCall : Mappable<SomaticVariantCall>
{
    SomaticVariantCall() = delete;
    SomaticVariantCall(const std::pair<VariantReference, Phred<double>>& p)
    : variant {p.first}, posterior {p.second} {}
    SomaticVariantCall(const Variant& variant, Phred<double> posterior)
    : variant {variant}, posterior {posterior} {}
    
    const GenomicRegion& mapped_region() const noexcept { return octopus::mapped_region(variant.get()); }
    
    VariantReference variant;
    Phred<double> posterior;
};

using SomaticVariantCalls = std::vector<SomaticVariantCall>;

struct GermlineGenotypeCall
{
    template <typename T>
    GermlineGenotypeCall(T&& genotype, Phred<double> posterior)
    : genotype {std::forward<T>(genotype)}, somatic {}, posterior {posterior} {}
    template <typename T, typename A>
    GermlineGenotypeCall(T&& genotype, A&& somatic, Phred<double> posterior)
    : genotype {std::forward<T>(genotype)}, somatic {std::forward<A>(somatic)}, posterior {posterior} {}
    
    Genotype<Allele> genotype;
    boost::optional<Allele> somatic;
    Phred<double> posterior;
};

using GermlineGenotypeCalls = std::vector<GermlineGenotypeCall>;

struct CancerGenotypeCall
{
    template <typename T>
    CancerGenotypeCall(T&& genotype, Phred<double> posterior)
    : genotype {std::forward<T>(genotype)}, posterior {posterior} {}
    
    CancerGenotype<Allele> genotype;
    Phred<double> posterior;
    std::unordered_map<SampleName, std::vector<std::pair<double, double>>> credible_regions;
};

using CancerGenotypeCalls = std::vector<CancerGenotypeCall>;

template <typename L>
auto find_map_genotype(const L& posteriors)
{
    return std::max_element(std::cbegin(posteriors),std::cend(posteriors),
                            [] (const auto& lhs, const auto& rhs) {
                                return lhs.second < rhs.second;
                            });
}

// germline variant posterior calculations

template <typename M>
Phred<double> marginalise(const Allele& allele, const M& genotype_posteriors)
{
    auto p = std::accumulate(std::cbegin(genotype_posteriors),
                             std::cend(genotype_posteriors),
                             0.0, [&allele] (const auto curr, const auto& p) {
                                 return curr + (contains(p.first, allele) ? 0.0 : p.second);
                             });
    return probability_to_phred(p);
}

template <typename M>
VariantPosteriors compute_candidate_posteriors(const std::vector<Variant>& candidates,
                                               const M& genotype_posteriors)
{
    VariantPosteriors result {};
    result.reserve(candidates.size());
    
    for (const auto& candidate : candidates) {
        result.emplace_back(candidate, marginalise(candidate.alt_allele(), genotype_posteriors));
    }
    
    return result;
}

// germline variant calling

bool contains_alt(const Genotype<Haplotype>& genotype_call, const VariantReference& candidate)
{
    return includes(genotype_call, candidate.get().alt_allele());
}

auto call_candidates(const VariantPosteriors& candidate_posteriors,
                     const Genotype<Haplotype>& genotype_call,
                     const Phred<double> min_posterior)
{
    GermlineVariantCalls calls {};
    calls.reserve(candidate_posteriors.size());
    
    std::vector<VariantReference> uncalled {};
    
    for (const auto& p : candidate_posteriors) {
        if (p.second >= min_posterior) {
            if (contains_alt(genotype_call, p.first)) {
                calls.emplace_back(p.first, p.second);
            }
        } else {
            uncalled.emplace_back(p.first);
        }
    }
    
    return std::make_pair(std::move(calls), std::move(uncalled));
}

// somatic variant posterior

template <typename M>
auto extract_likely_cancer_genotypes(const M& cancer_genotype_posteriors,
                                     const double min_posterior = 0.0001)
{
    std::deque<std::pair<CancerGenotype<Haplotype>, double>> result {};
    
    std::copy_if(std::cbegin(cancer_genotype_posteriors), std::cend(cancer_genotype_posteriors),
                 std::back_inserter(result),
                 [min_posterior] (const auto& p) { return p.second > min_posterior; });
    
    return result;
}

template <typename M>
auto compute_somatic_variant_posteriors(const std::vector<VariantReference>& candidates,
                                        const M& cancer_genotype_posteriors,
                                        const double somatic_posterior,
                                        const double somatic_model_posterior)
{
    VariantPosteriors result {};
    result.reserve(candidates.size());
    
    for (const auto& candidate : candidates) {
        const auto& allele = candidate.get().alt_allele();
        
        const auto p = std::accumulate(std::cbegin(cancer_genotype_posteriors),
                                       std::cend(cancer_genotype_posteriors),
                                       0.0, [&allele] (const auto curr, const auto& p) {
                                           if (p.first.somatic_element().contains(allele)
                                               && !contains(p.first.germline_genotype(), allele)) {
                                               return curr + p.second;
                                           }
                                           return curr;
                                       });
        
        result.emplace_back(candidate, probability_to_phred(1.0 - somatic_model_posterior * p * somatic_posterior));
    }
    
    return result;
}

auto call_somatic_variants(const VariantPosteriors& somatic_variant_posteriors,
                           const CancerGenotype<Haplotype>& called_genotype,
                           const Phred<double> min_posterior)
{
    SomaticVariantCalls result {};
    result.reserve(somatic_variant_posteriors.size());
    
    std::copy_if(std::begin(somatic_variant_posteriors), std::end(somatic_variant_posteriors),
                 std::back_inserter(result),
                 [min_posterior, &called_genotype] (const auto& p) {
                     return p.second >= min_posterior && includes(called_genotype, p.first.get().alt_allele());
                 });
    
    return result;
}

template <typename T>
auto compute_marginal_credible_interval(const T& alphas, const double mass)
{
    const auto a0 = std::accumulate(std::cbegin(alphas), std::cend(alphas), 0.0);
    
    std::vector<std::pair<double, double>> result {};
    result.reserve(alphas.size());
    
    for (const auto& alpha : alphas) {
        result.push_back(maths::beta_hdi(alpha, a0 - alpha, mass));
    }
    
    return result;
}

using CredibleRegionMap = std::unordered_map<SampleName, std::vector<std::pair<double, double>>>;

template <typename M>
auto compute_marginal_credible_intervals(const M& alphas, const double mass)
{
    CredibleRegionMap result {};
    result.reserve(alphas.size());
    
    for (const auto& p : alphas) {
        result.emplace(p.first, compute_marginal_credible_interval(p.second, mass));
    }
    
    return result;
}

template <typename M, typename T>
auto call_somatic_genotypes(const CancerGenotype<Haplotype>& called_genotype,
                            const std::vector<GenomicRegion>& called_somatic_regions,
                            const M& genotype_posteriors,
                            const T& credible_regions)
{
    CancerGenotypeCalls result {};
    result.reserve(called_somatic_regions.size());
    
    for (const auto& region : called_somatic_regions) {
        auto spliced_genotype = splice<Allele>(called_genotype, region);
        
        const auto inv_posterior = std::accumulate(std::cbegin(genotype_posteriors),
                                                   std::cend(genotype_posteriors), 0.0,
                                                   [&spliced_genotype] (const double curr, const auto& p) {
                                                       return curr + (contains(p.first, spliced_genotype) ? 0.0 : p.second);
                                                   });
        
        result.emplace_back(std::move(spliced_genotype), Phred<double> {inv_posterior});
        result.back().credible_regions = credible_regions;
    }
    
    return result;
}

// output

octopus::VariantCall::GenotypeCall convert(GermlineGenotypeCall call)
{
    return octopus::VariantCall::GenotypeCall {std::move(call.genotype), call.posterior};
}

std::unique_ptr<octopus::VariantCall>
transform_germline_call(GermlineVariantCall&& variant_call, GermlineGenotypeCall&& genotype_call,
                        const std::vector<SampleName>& samples,
                        const std::vector<SampleName>& somatic_samples)
{
    std::vector<std::pair<SampleName, Call::GenotypeCall>> genotypes {};
    
    for (const auto& sample : samples) {
        if (std::find(std::cbegin(somatic_samples), std::cend(somatic_samples), sample) == std::cend(somatic_samples)) {
            genotypes.emplace_back(sample, convert(genotype_call));
        } else {
            auto copy = genotype_call;
            copy.genotype.emplace(*copy.somatic);
            genotypes.emplace_back(sample, convert(std::move(copy)));
        }
    }
    
    return std::make_unique<octopus::GermlineVariantCall>(variant_call.variant.get(),
                                                          std::move(genotypes),
                                                          variant_call.posterior);
}

auto transform_somatic_calls(SomaticVariantCalls&& somatic_calls, CancerGenotypeCalls&& genotype_calls,
                             const std::vector<SampleName>& somatic_samples)
{
    std::vector<std::unique_ptr<octopus::VariantCall>> result {};
    result.reserve(somatic_calls.size());
    
    std::transform(std::make_move_iterator(std::begin(somatic_calls)),
                   std::make_move_iterator(std::end(somatic_calls)),
                   std::make_move_iterator(std::begin(genotype_calls)),
                   std::back_inserter(result),
                   [&somatic_samples] (auto&& variant_call, auto&& genotype_call) -> std::unique_ptr<octopus::VariantCall> {
                       std::unordered_map<SampleName, SomaticCall::GenotypeCredibleRegions> credible_regions {};
                       
                       for (const auto& p : genotype_call.credible_regions) {
                           SomaticCall::GenotypeCredibleRegions sample_credible_regions {};
                           
                           sample_credible_regions.germline.reserve(p.second.size() - 1);
                           
                           std::copy(std::cbegin(p.second), std::prev(std::cend(p.second)),
                                     std::back_inserter(sample_credible_regions.germline));
                           
                           if (std::find(std::cbegin(somatic_samples), std::cend(somatic_samples), p.first) != std::cend(somatic_samples)) {
                               sample_credible_regions.somatic = p.second.back();
                           }
                           
                           credible_regions.emplace(p.first, std::move(sample_credible_regions));
                       }
                       
                       return std::make_unique<SomaticCall>(variant_call.variant.get(),
                                                            std::move(genotype_call.genotype),
                                                            genotype_call.posterior,
                                                            std::move(credible_regions),
                                                            variant_call.posterior);
                   });
    
    return result;
}
} // namespace

std::vector<std::unique_ptr<VariantCall>>
CancerCaller::call_variants(const std::vector<Variant>& candidates, const Latents& latents) const
{
    const auto model_posteriors = calculate_model_posteriors(latents);
    
    if (debug_log_) {
        stream(*debug_log_) << "Germline model posterior: " << model_posteriors.germline;
        stream(*debug_log_) << "CNV model posterior:      " << model_posteriors.cnv;
        stream(*debug_log_) << "Somatic model posterior:  " << model_posteriors.somatic;
        
        auto map_cnv = find_map_genotype(latents.cnv_model_inferences_.posteriors.genotype_probabilities);
        auto cnv_log = stream(*debug_log_);
        cnv_log << "MAP CNV genotype is: ";
        debug::print_variant_alleles(cnv_log, map_cnv->first);
        auto somatic_log = stream(*debug_log_);
        auto map_somatic = find_map_genotype(latents.somatic_model_inferences_.posteriors.genotype_probabilities);
        somatic_log << "MAP cancer genotype is: ";
        debug::print_variant_alleles(somatic_log, map_somatic->first);
    }
    
    const auto germline_genotype_posteriors = calculate_germline_genotype_posteriors(latents, model_posteriors);
    
    const auto germline_candidate_posteriors = compute_candidate_posteriors(candidates, germline_genotype_posteriors);
    
    const auto& called_germline_genotype = find_map_genotype(germline_genotype_posteriors)->first;
    
    GermlineVariantCalls germline_variant_calls;
    std::vector<VariantReference> uncalled_germline_candidates;
    std::tie(germline_variant_calls, uncalled_germline_candidates) = call_candidates(germline_candidate_posteriors,
                                                                                     called_germline_genotype,
                                                                                     parameters_.min_variant_posterior);
    
    const auto sample_somatic_inv_posteriors = calculate_probability_samples_not_somatic(latents);
    
    const auto somatic_posterior = calculate_somatic_probability(sample_somatic_inv_posteriors,
                                                                 model_posteriors);
    
    std::vector<std::unique_ptr<octopus::VariantCall>> result {};
    
    boost::optional<Haplotype> called_somatic_haplotype {};
    std::vector<SampleName> somatic_samples {};
    
    if (somatic_posterior >= parameters_.min_somatic_posterior) {
        const auto& cancer_genotype_posteriors = latents.somatic_model_inferences_.posteriors.genotype_probabilities;
        
        auto reduced_cancer_genotype_posteriors = extract_likely_cancer_genotypes(cancer_genotype_posteriors);
        
        auto somatic_allele_posteriors = compute_somatic_variant_posteriors(uncalled_germline_candidates,
                                                                            reduced_cancer_genotype_posteriors,
                                                                            somatic_posterior.probability_true(),
                                                                            model_posteriors.somatic);
        
        const auto& called_cancer_genotype = find_map_genotype(cancer_genotype_posteriors)->first;
        
        auto somatic_variant_calls = call_somatic_variants(somatic_allele_posteriors,
                                                           called_cancer_genotype,
                                                           parameters_.min_somatic_posterior);
        
        const auto& somatic_alphas = latents.somatic_model_inferences_.posteriors.alphas;
        
        const auto credible_regions = compute_marginal_credible_intervals(somatic_alphas, parameters_.credible_mass);
        
        if (!somatic_variant_calls.empty()) {
            for (const auto& p : credible_regions) {
                if (p.second.back().first >= parameters_.min_somatic_frequency) {
                    somatic_samples.push_back(p.first);
                }
            }
            
            if (somatic_samples.empty()) {
                somatic_variant_calls.clear();
                somatic_variant_calls.shrink_to_fit();
            } else {
                called_somatic_haplotype = called_cancer_genotype.somatic_element();
            }
        }
        
        const auto called_somatic_regions = extract_regions(somatic_variant_calls);
        
        auto cancer_genotype_calls = call_somatic_genotypes(called_cancer_genotype,
                                                            called_somatic_regions,
                                                            reduced_cancer_genotype_posteriors,
                                                            credible_regions);
        
        result = transform_somatic_calls(std::move(somatic_variant_calls),
                                         std::move(cancer_genotype_calls),
                                         somatic_samples);
    }
    
    const auto called_germline_regions = extract_regions(germline_variant_calls);
    
    GermlineGenotypeCalls germline_genotype_calls {};
    germline_genotype_calls.reserve(called_germline_regions.size());
    
    for (const auto& region : called_germline_regions) {
        auto spliced_genotype = splice<Allele>(called_germline_genotype, region);
        
        const auto inv_posterior = std::accumulate(std::cbegin(germline_genotype_posteriors),
                                                   std::cend(germline_genotype_posteriors), 0.0,
                                                   [&called_germline_genotype] (const double curr, const auto& p) {
                                                       return curr + (contains(p.first, called_germline_genotype) ? 0.0 : p.second);
                                                   });
        
        if (called_somatic_haplotype) {
            germline_genotype_calls.emplace_back(std::move(spliced_genotype),
                                                 splice<Allele>(*called_somatic_haplotype, region),
                                                 Phred<double> {inv_posterior});
        } else {
            germline_genotype_calls.emplace_back(std::move(spliced_genotype), Phred<double> {inv_posterior});
        }
    }
    
    result.reserve(result.size() + germline_variant_calls.size());
    
    const auto it = std::end(result);
    
    std::transform(std::make_move_iterator(std::begin(germline_variant_calls)),
                   std::make_move_iterator(std::end(germline_variant_calls)),
                   std::make_move_iterator(std::begin(germline_genotype_calls)),
                   std::back_inserter(result),
                   [this, &somatic_samples] (auto&& variant_call, auto&& genotype_call) {
                       return transform_germline_call(std::move(variant_call), std::move(genotype_call),
                                                      samples_, somatic_samples);
                   });
    
    std::inplace_merge(std::begin(result), it, std::end(result),
                       [] (const auto& lhs, const auto& rhs) {
                           return *lhs < *rhs;
                       });
    
    return result;
}
        
CancerCaller::ModelPriors
CancerCaller::get_model_priors() const
{
    const double cnv_model_prior      {0.01};
    const double somatic_model_prior  {parameters_.somatic_mutation_rate};
    const double germline_model_prior {std::max(0.0, 1.0 - (cnv_model_prior + somatic_model_prior))};
    return {germline_model_prior, cnv_model_prior, somatic_model_prior};
}

CancerCaller::ModelPosteriors
CancerCaller::calculate_model_posteriors(const Latents& inferences) const
{
    const auto& germline_inferences = inferences.germline_model_inferences_;
    const auto& cnv_inferences      = inferences.cnv_model_inferences_;
    const auto& somatic_inferences  = inferences.somatic_model_inferences_;
    
    const auto& model_priors = inferences.model_priors_;
    
    const auto germline_model_jlp = std::log(model_priors.germline) + germline_inferences.log_evidence;
    const auto cnv_model_jlp      = std::log(model_priors.cnv) + cnv_inferences.approx_log_evidence;
    const auto somatic_model_jlp  = std::log(model_priors.somatic) + somatic_inferences.approx_log_evidence;
    
    const auto norm = maths::log_sum_exp(germline_model_jlp, cnv_model_jlp, somatic_model_jlp);
    
    auto germline_model_posterior = std::exp(germline_model_jlp - norm);
    auto cnv_model_posterior      = std::exp(cnv_model_jlp - norm);
    auto somatic_model_posterior  = std::exp(somatic_model_jlp - norm);
    
    return {germline_model_posterior, cnv_model_posterior, somatic_model_posterior};
}

CancerCaller::GermlineGenotypeProbabilityMap
CancerCaller::calculate_germline_genotype_posteriors(const Latents& inferences,
                                                     const ModelPosteriors& model_posteriors) const
{
    GermlineGenotypeProbabilityMap result {inferences.germline_genotypes_.size()};
    
    std::transform(std::cbegin(inferences.germline_genotypes_), std::cend(inferences.germline_genotypes_),
                   std::cbegin(inferences.germline_model_inferences_.posteriors.genotype_probabilities),
                   std::inserter(result, std::begin(result)),
                   [&model_posteriors] (const auto& genotype, const auto& posterior) {
                       return std::make_pair(genotype, model_posteriors.germline * posterior);
                   });
    
    for (const auto& p : inferences.cnv_model_inferences_.posteriors.genotype_probabilities) {
        result[p.first] += model_posteriors.cnv * p.second;
    }
    
    for (const auto& p : inferences.somatic_model_inferences_.posteriors.genotype_probabilities) {
        result[p.first.germline_genotype()] += model_posteriors.somatic * p.second;
    }
    
    return result;
}

CancerCaller::ProbabilityVector
CancerCaller::calculate_probability_samples_not_somatic(const Latents& inferences) const
{
    std::vector<double> result(samples_.size());
    
    const auto ploidy = parameters_.ploidy;
    
    const auto& posterior_alphas = inferences.somatic_model_inferences_.posteriors.alphas;
    
    std::transform(std::cbegin(posterior_alphas), std::cend(posterior_alphas),
                   std::begin(result), [this, ploidy] (const auto& p) {
                       const auto a0 = std::accumulate(std::cbegin(p.second),
                                                       std::prev(std::cend(p.second)),
                                                       0.0);
                       return maths::beta_cdf(p.second.back(), a0, parameters_.min_somatic_frequency);
                   });
    
    return result;
}

Phred<double> CancerCaller::calculate_somatic_probability(const ProbabilityVector& sample_somatic_posteriors,
                                                          const ModelPosteriors& model_posteriors) const
{
    auto result = 1.0 - std::accumulate(std::cbegin(sample_somatic_posteriors),
                                        std::cend(sample_somatic_posteriors),
                                        1.0, std::multiplies<> {});
    
    result *= model_posteriors.somatic;
    
    return probability_to_phred(1 - result);
}

std::vector<std::unique_ptr<ReferenceCall>>
CancerCaller::call_reference(const std::vector<Allele>& alleles,
                             const Caller::Latents& latents,
                             const ReadMap& reads) const
{
    return {};
}

} // namespace octopus
