// Copyright (c) 2017 Daniel Cooke
// Use of this source code is governed by the MIT license that can be found in the LICENSE file.

#ifndef randomiser_hpp
#define randomiser_hpp

#include <vector>
#include <functional>
#include <memory>

#include "basics/aligned_read.hpp"
#include "core/types/variant.hpp"
#include "variant_generator.hpp"


namespace octopus {
    
class GenomicRegion;
class ReferenceGenome;

namespace coretools {

class Randomiser : public VariantGenerator
{
public:
    struct Options
    {
        Variant::MappingDomain::Size max_variant_size = 100;
    };
    
    Randomiser() = delete;
    
    Randomiser(const ReferenceGenome& reference, Options options);
    
    Randomiser(const Randomiser&)            = default;
    Randomiser& operator=(const Randomiser&) = default;
    Randomiser(Randomiser&&)                 = default;
    Randomiser& operator=(Randomiser&&)      = default;
    
    ~Randomiser() override = default;
    
private:
    using VariantGenerator::VectorIterator;
    using VariantGenerator::FlatSetIterator;
    
    std::unique_ptr<VariantGenerator> do_clone() const override;
    
    void do_add_reads(const SampleName& sample, VectorIterator first, VectorIterator last) override;
    void do_add_reads(const SampleName& sample, FlatSetIterator first, FlatSetIterator last) override;
    
    std::vector<Variant> do_generate_variants(const GenomicRegion& region) override;
    
    std::string name() const override;
    
    std::reference_wrapper<const ReferenceGenome> reference_;
    Options options_;
    AlignedRead::MappingDomain::Size max_read_size_;
};

} // namespace coretools
} // namespace octopus

#endif
