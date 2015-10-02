//
//  haplotype_prior_model.hpp
//  Octopus
//
//  Created by Daniel Cooke on 26/08/2015.
//  Copyright (c) 2015 Oxford University. All rights reserved.
//

#ifndef Octopus_haplotype_prior_model_hpp
#define Octopus_haplotype_prior_model_hpp

#include <vector>
#include <unordered_map>

class Haplotype;

namespace Octopus
{

class HaplotypePriorModel
{
public:
    HaplotypePriorModel()  = default;
    ~HaplotypePriorModel() = default;
    
    HaplotypePriorModel(const HaplotypePriorModel&)            = default;
    HaplotypePriorModel& operator=(const HaplotypePriorModel&) = default;
    HaplotypePriorModel(HaplotypePriorModel&&)                 = default;
    HaplotypePriorModel& operator=(HaplotypePriorModel&&)      = default;
    
    // ln p(to | from)
    double evaluate(const Haplotype& to, const Haplotype& from);
    
    std::unordered_map<Haplotype, double> evaluate(const std::vector<Haplotype>& haplotypes, const Haplotype& reference);
    
private:
    double transition_rate_   = 0.000222;
    double transversion_rate_ = 0.000111;
};

} // namespace Octopus

#endif
