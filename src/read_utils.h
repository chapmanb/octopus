//
//  read_utils.h
//  Octopus
//
//  Created by Daniel Cooke on 09/04/2015.
//  Copyright (c) 2015 Oxford University. All rights reserved.
//

#ifndef __Octopus__read_utils__
#define __Octopus__read_utils__

#include <vector>
#include <unordered_map>
#include <iterator>  // std::begin, std::end, std::make_move_iterator
#include <utility>   // std::move
#include <algorithm> // std::min_element, std::max_element, std::transform

#include "aligned_read.h"
#include "read_filter.h"

template <typename T, typename Container>
using ReadMap = std::unordered_map<T, Container>;

template <typename Container>
void reserve_if_enabled(Container& container, typename Container::size_type n) {}

template <typename T>
void reserve_if_enabled(std::vector<T>& container, typename std::vector<T>::size_type n) { container.reserve(n); }

template <typename Container>
void shrink_to_fit_if_enabled(Container& container) {}

template <typename T>
void shrink_to_fit_if_enabled(std::vector<T>& container) { container.shrink_to_fit(); }

template <typename T, typename Container, typename ReadFilter>
std::pair<ReadMap<T, Container>, ReadMap<T, Container>>
filter_reads(ReadMap<T, Container>&& the_reads, ReadFilter& a_read_filter)
{
    ReadMap<T, Container> good_read_map {}, bad_read_map {};
    good_read_map.reserve(the_reads.size());
    bad_read_map.reserve(the_reads.size());
    
    for (auto& sample_reads : the_reads) {
        Container good_reads {}, bad_reads {};
        
        reserve_if_enabled(good_reads, sample_reads.second.size());
        reserve_if_enabled(bad_reads, sample_reads.second.size() / 10); // arbitrarily chosen
        
        a_read_filter.filter_reads(std::make_move_iterator(std::begin(sample_reads.second)),
                                   std::make_move_iterator(std::end(sample_reads.second)),
                                   ContextBackInserter(good_reads), ContextBackInserter(bad_reads));
        
        sample_reads.second.clear();
        shrink_to_fit_if_enabled(good_reads);
        shrink_to_fit_if_enabled(bad_reads);
        
        good_read_map.emplace(sample_reads.first, std::move(good_reads));
        bad_read_map.emplace(std::move(sample_reads.first), std::move(bad_reads));
    }
    
    return {good_read_map, bad_read_map};
}

std::vector<unsigned> positional_coverage(const std::vector<AlignedRead>& reads, const GenomicRegion& a_region);

unsigned min_coverage(const std::vector<AlignedRead>& reads, const GenomicRegion& a_region);

unsigned max_coverage(const std::vector<AlignedRead>& reads, const GenomicRegion& a_region);

double mean_coverage(const std::vector<AlignedRead>& reads, const GenomicRegion& a_region);

double stdev_coverage(const std::vector<AlignedRead>& reads, const GenomicRegion& a_region);

template <typename T, typename Container>
unsigned min_coverage(const ReadMap<T, Container>& reads, const GenomicRegion& a_region)
{
    std::vector<unsigned> sample_min_coverages(reads.size(), 0);
    
    std::transform(std::cbegin(reads), std::cend(reads), sample_min_coverages.begin(),
                   [&a_region] (const auto& sample_reads) {
                       return min_coverage(sample_reads.second, a_region);
                   });
    
    return *std::min_element(sample_min_coverages.cbegin(), sample_min_coverages.cend());
}

template <typename T, typename Container>
unsigned max_coverage(const ReadMap<T, Container>& reads, const GenomicRegion& a_region)
{
    std::vector<unsigned> sample_max_coverages(reads.size(), 0);
    
    std::transform(std::cbegin(reads), std::cend(reads), sample_max_coverages.begin(),
                   [&a_region] (const auto& sample_reads) {
                       return max_coverage(sample_reads.second, a_region);
                   });
    
    return *std::max_element(sample_max_coverages.cbegin(), sample_max_coverages.cend());
}

std::vector<GenomicRegion> find_high_coverage_regions(const std::vector<AlignedRead>& reads,
                                                      const GenomicRegion& a_region, unsigned maximum_coverage);

template <typename SampleIdType, typename Container>
std::unordered_map<SampleIdType, std::vector<GenomicRegion>>
find_high_coverage_regions(const ReadMap<SampleIdType, Container>& reads,
                           const GenomicRegion& a_region, unsigned maximum_coverage)
{
    std::unordered_map<SampleIdType, std::vector<GenomicRegion>> result {};
    result.reserve(reads.size());
    
    for (const auto& sample_reads : reads) {
        result.emplace(sample_reads.first, find_high_coverage_regions(sample_reads.second, a_region, maximum_coverage));
    }
    
    return result;
}

std::vector<AlignedRead> downsample(const std::vector<AlignedRead>& reads, const GenomicRegion& a_region,
                                    unsigned maximum_coverage, unsigned minimum_downsample_coverage);

template <typename T, typename Container>
ReadMap<T, Container> downsample(ReadMap<T, Container>&& reads, unsigned max_coverage_per_sample)
{
    return reads;
}

#endif /* defined(__Octopus__read_utils__) */
