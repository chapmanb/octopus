//
//  caller_factory.hpp
//  Octopus
//
//  Created by Daniel Cooke on 21/01/2016.
//  Copyright © 2016 Oxford University. All rights reserved.
//

#ifndef caller_factory_hpp
#define caller_factory_hpp

#include <unordered_map>
#include <memory>

#include "genomic_region.hpp"
#include "caller.hpp"
#include "variant_generator.hpp"
#include "caller_builder.hpp"

namespace octopus {

class ReferenceGenome;
class ReadPipe;

class CallerFactory
{
public:
    using ContigName = GenomicRegion::ContigName;
    
    CallerFactory() = delete;
    
    CallerFactory(CallerBuilder template_builder, unsigned default_ploidy);
    
    CallerFactory(const CallerFactory&)            = default;
    CallerFactory& operator=(const CallerFactory&) = default;
    CallerFactory(CallerFactory&&)                 = default;
    CallerFactory& operator=(CallerFactory&&)      = default;
    
    ~CallerFactory() = default;
    
    CallerFactory& set_reference(const ReferenceGenome& reference) noexcept;
    CallerFactory& set_read_pipe(ReadPipe& read_pipe) noexcept;
    
    CallerFactory& set_contig_ploidy(const ContigName& contig, unsigned ploidy);
    
    std::unique_ptr<Caller> make(const ContigName& contig) const;
    
private:
    mutable CallerBuilder template_builder_;
    std::unordered_map<ContigName, unsigned> contig_ploidies_;
    unsigned default_ploidy_;
};

} // namespace octopus

#endif /* caller_factory_hpp */
