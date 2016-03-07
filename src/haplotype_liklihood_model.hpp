//
//  haplotypeliklihood_model.hpp
//  Octopus
//
//  Created by Daniel Cooke on 25/09/2015.
//  Copyright © 2015 Oxford University. All rights reserved.
//

#ifndef haplotype_liklihood_model_hpp
#define haplotype_liklihood_model_hpp

#include <vector>
#include <iterator>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <functional>

#include "common.hpp"
#include "haplotype.hpp"
#include "read_indel_error_model.hpp"
#include "pair_hmm.hpp"

#include "timers.hpp"

class AlignedRead;

namespace Octopus
{
    class HaplotypeLikelihoodModel
    {
    public:
        enum class FlankState { Clear, Unclear, LeftUnclear, RightUnclear };
        
        using MapPositionItr = std::vector<std::size_t>::const_iterator;
        
        HaplotypeLikelihoodModel() = delete;
        
        HaplotypeLikelihoodModel(const Haplotype& haplotype, FlankState flank_state);
        
        ~HaplotypeLikelihoodModel() = default;
        
        HaplotypeLikelihoodModel(const HaplotypeLikelihoodModel&)            = default;
        HaplotypeLikelihoodModel& operator=(const HaplotypeLikelihoodModel&) = default;
        HaplotypeLikelihoodModel(HaplotypeLikelihoodModel&&)                 = default;
        HaplotypeLikelihoodModel& operator=(HaplotypeLikelihoodModel&&)      = default;
        
        // ln p(read | haplotype, model)
        double log_probability(const AlignedRead& read,
                               MapPositionItr first_mapping_position,
                               MapPositionItr last_mapping_position) const;
        
    private:
        ReadIndelErrorModel indel_error_model_;
        
        std::reference_wrapper<const Haplotype> haplotype_;
        
        std::vector<std::int8_t> haplotype_gap_open_penalities_;
        
        PairHMM::Model model_;
    };
} // namespace Octopus

#endif /* haplotype_liklihood_model_hpp */