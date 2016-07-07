//
//  pair_hmm.hpp
//  pair_hmm
//
//  Created by Daniel Cooke on 14/12/2015.
//  Copyright © 2015 Oxford University. All rights reserved.
//

#ifndef pair_hmm_hpp
#define pair_hmm_hpp

#include <string>
#include <vector>
#include <cstddef>

namespace PairHMM
{
    unsigned min_flank_pad() noexcept;
    
    struct Model
    {
        using PenaltyType = std::int8_t;
        
        const std::vector<char>& snv_mask;
        const std::vector<PenaltyType>& snv_priors;
        const std::vector<PenaltyType>& gap_open_penalties;
        short gap_extend;
        short nuc_prior = 2;
        std::size_t lhs_flank_size = 0, rhs_flank_size = 0;
    };
    
    // p(target | truth, target_qualities, model)
    double align(const std::string& truth, const std::string& target,
                 const std::vector<std::uint8_t>& target_qualities,
                 std::size_t target_offset,
                 const Model& model);
} // namespace PairHMM

#endif /* pair_hmm_hpp */
