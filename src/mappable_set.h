//
//  mappable_set.h
//  Octopus
//
//  Created by Daniel Cooke on 19/07/2015.
//  Copyright (c) 2015 Oxford University. All rights reserved.
//

#ifndef Octopus_mappable_set_h
#define Octopus_mappable_set_h

#include <memory>     // std::allocator
#include <functional> // std::less
#include <algorithm>  // std::max, std::minmax
#include <iterator>   // std::begin, std::end, std::cbegin, std::cend
#include <boost/container/flat_set.hpp>

#include "genomic_region.h"
#include "mappable.h"
#include "mappable_ranges.h"
#include "mappable_algorithms.h"

#include <iostream> // TEST

template <typename MappableType, typename Allocator = std::allocator<MappableType>>
class MappableSet
{
protected:
    using base_t = boost::container::flat_multiset<MappableType, std::less<MappableType>, Allocator>;
    
public:
    using allocator_type  = typename base_t::allocator_type;
    using value_type      = typename base_t::value_type;
    using reference       = typename base_t::reference ;
    using const_reference = typename base_t::const_reference;
    using difference_type = typename base_t::difference_type ;
    using size_type       = typename base_t::size_type ;
    
    using iterator               = typename base_t::iterator;
    using const_iterator         = typename base_t::const_iterator;
    using reverse_iterator       = typename base_t::reverse_iterator;
    using const_reverse_iterator = typename base_t::const_reverse_iterator;
    
    MappableSet() = default;
    template <typename InputIterator>
    MappableSet(InputIterator first, InputIterator second);
    ~MappableSet() = default;
    
    MappableSet(const MappableSet&)            = default;
    MappableSet& operator=(const MappableSet&) = default;
    MappableSet(MappableSet&&)                 = default;
    MappableSet& operator=(MappableSet&&)      = default;
    
    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;
    iterator end();
    const_iterator end() const;
    const_iterator cend() const;
    reverse_iterator rbegin();
    const_reverse_iterator rbegin() const;
    const_reverse_iterator crbegin() const;
    reverse_iterator rend();
    const_reverse_iterator rend() const;
    const_reverse_iterator crend() const;
    
    template <typename ...Args>
    iterator emplace(Args...);
    iterator insert(const MappableType&);
    iterator insert(MappableType&&);
    template <typename InputIterator>
    void insert(InputIterator, InputIterator);
    iterator insert(std::initializer_list<MappableType>);
    iterator erase(const_iterator);
    iterator erase(const MappableType&);
    iterator erase(const_iterator, const_iterator);
    //void erase(OverlapRange<const_iterator>);
    //void erase(ContainedRange<const_iterator>);
    void clear();
    
    void swap(const MappableSet&);
    size_type size() const noexcept;
    size_type capacity() const noexcept;
    size_type max_size() const noexcept;
    bool empty() const noexcept;
    void reserve(size_type n);
    void shrink_to_fit();
    
    allocator_type get_allocator();
    
    template <typename MappableType_>
    bool has_overlapped(const MappableType_& mappable) const;
    template <typename MappableType_>
    bool has_overlapped(iterator first, iterator last, const MappableType_& mappable) const;
    template <typename MappableType_>
    bool has_overlapped(const_iterator first, const_iterator last, const MappableType_& mappable) const;
    
    template <typename MappableType_>
    size_type count_overlapped(const MappableType_& mappable) const;
    template <typename MappableType_>
    size_type count_overlapped(iterator first, iterator last, const MappableType_& mappable) const;
    template <typename MappableType_>
    size_type count_overlapped(const_iterator first, const_iterator last, const MappableType_& mappable) const;
    
    template <typename MappableType_>
    OverlapRange<const_iterator> overlap_range(const MappableType_& mappable) const;
    template <typename MappableType_>
    OverlapRange<iterator> overlap_range(iterator first, iterator last, const MappableType_& mappable) const;
    template <typename MappableType_>
    OverlapRange<const_iterator> overlap_range(const_iterator first, const_iterator last, const MappableType_& mappable) const;
    
    template <typename MappableType_>
    bool has_contained(const MappableType_& mappable) const;
    template <typename MappableType_>
    bool has_contained(iterator first, iterator last, const MappableType_& mappable) const;
    template <typename MappableType_>
    bool has_contained(const_iterator first, const_iterator last, const MappableType_& mappable) const;
    
    template <typename MappableType_>
    size_type count_contained(const MappableType_& mappable) const;
    template <typename MappableType_>
    size_type count_contained(iterator first, iterator last, const MappableType_& mappable) const;
    template <typename MappableType_>
    size_type count_contained(const_iterator first, const_iterator last, const MappableType_& mappable) const;
    
    template <typename MappableType_>
    ContainedRange<const_iterator> contained_range(const MappableType_& mappable) const;
    template <typename MappableType_>
    ContainedRange<iterator> contained_range(iterator first, iterator last, const MappableType_& mappable) const;
    template <typename MappableType_>
    ContainedRange<const_iterator> contained_range(const_iterator first, const_iterator last, const MappableType_& mappable) const;
    
    template <typename MappableType1_, typename MappableType2_>
    bool has_shared(const MappableType1_& mappable1, const MappableType2_& mappable2) const;
    template <typename MappableType1_, typename MappableType2_>
    bool has_shared(iterator first, iterator last, const MappableType1_& mappable1, const MappableType2_& mappable2) const;
    template <typename MappableType1_, typename MappableType2_>
    bool has_shared(const_iterator first, const_iterator last, const MappableType1_& mappable1, const MappableType2_& mappable2) const;
    
    template <typename MappableType1_, typename MappableType2_>
    size_type count_shared(const MappableType1_& mappable1, const MappableType2_& mappable2) const;
    template <typename MappableType1_, typename MappableType2_>
    size_type count_shared(iterator first, iterator last, const MappableType1_& mappable1, const MappableType2_& mappable2) const;
    template <typename MappableType1_, typename MappableType2_>
    size_type count_shared(const_iterator first, const_iterator last, const MappableType1_& mappable1, const MappableType2_& mappable2) const;
    
    template <typename MappableType1_, typename MappableType2_>
    SharedRange<const_iterator> shared_range(const MappableType1_& mappable1, const MappableType2_& mappable2) const;
    template <typename MappableType1_, typename MappableType2_>
    SharedRange<iterator> shared_range(iterator first, iterator last, const MappableType1_& mappable1, const MappableType2_& mappable2) const;
    template <typename MappableType1_, typename MappableType2_>
    SharedRange<const_iterator> shared_range(const_iterator first, const_iterator last, const MappableType1_& mappable1, const MappableType2_& mappable2) const;
    
private:
    base_t elements_;
    
    bool is_bidirectionally_sorted_;
    GenomicRegion::SizeType max_element_size_;
};

template <typename MappableType, typename Allocator>
template <typename InputIterator>
MappableSet<MappableType, Allocator>::MappableSet(InputIterator first, InputIterator second)
:
elements_ {first, second},
is_bidirectionally_sorted_ {is_bidirectionally_sorted(std::cbegin(elements_), std::cend(elements_))},
max_element_size_ {::size(*largest_element(std::cbegin(elements_), std::cend(elements_)))}
{}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::iterator
MappableSet<MappableType, Allocator>::begin()
{
    return elements_.begin();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::const_iterator
MappableSet<MappableType, Allocator>::begin() const
{
    return elements_.begin();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::const_iterator
MappableSet<MappableType, Allocator>::cbegin() const
{
    return elements_.cbegin();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::iterator
MappableSet<MappableType, Allocator>::end()
{
    return elements_.end();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::const_iterator
MappableSet<MappableType, Allocator>::end() const
{
    return elements_.end();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::const_iterator
MappableSet<MappableType, Allocator>::cend() const
{
    return elements_.cend();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::reverse_iterator
MappableSet<MappableType, Allocator>::rbegin()
{
    return elements_.rbegin();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::const_reverse_iterator
MappableSet<MappableType, Allocator>::rbegin() const
{
    return elements_.rbegin();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::const_reverse_iterator
MappableSet<MappableType, Allocator>::crbegin() const
{
    return elements_.crbegin();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::reverse_iterator
MappableSet<MappableType, Allocator>::rend()
{
    return elements_.rend();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::const_reverse_iterator
MappableSet<MappableType, Allocator>::rend() const
{
    return elements_.rend();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::const_reverse_iterator
MappableSet<MappableType, Allocator>::crend() const
{
    return elements_.crend();
}

template <typename MappableType, typename Allocator>
template <typename ...Args>
typename MappableSet<MappableType, Allocator>::iterator
MappableSet<MappableType, Allocator>::emplace(Args... args)
{
    auto it = elements_.emplace(std::forward<Args>(args)...);
    if (is_bidirectionally_sorted_) {
        auto overlapped = overlap_range(*it);
        is_bidirectionally_sorted_ = is_bidirectionally_sorted(overlapped.begin(), overlapped.end());
    }
    max_element_size_ = std::max(max_element_size_, ::size(*it));
    return it;
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::iterator
MappableSet<MappableType, Allocator>::insert(const MappableType& m)
{
    auto it = elements_.insert(m);
    if (is_bidirectionally_sorted_) {
        auto overlapped = overlap_range(*it);
        is_bidirectionally_sorted_ = is_bidirectionally_sorted(overlapped.begin(), overlapped.end());
    }
    max_element_size_ = std::max(max_element_size_, ::size(*it));
    return it;
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::iterator
MappableSet<MappableType, Allocator>::insert(MappableType&& m)
{
    auto it = elements_.insert(std::move(m));
    if (is_bidirectionally_sorted_) {
        auto overlapped = overlap_range(*it);
        is_bidirectionally_sorted_ = is_bidirectionally_sorted(overlapped.begin(), overlapped.end());
    }
    max_element_size_ = std::max(max_element_size_, ::size(*it));
    return it;
}

template <typename MappableType, typename Allocator>
template <typename InputIterator>
void
MappableSet<MappableType, Allocator>::insert(InputIterator first, InputIterator last)
{
    max_element_size_ = std::max(max_element_size_, ::size(*largest_element(first, last)));
    elements_.insert(first, last);
    if (is_bidirectionally_sorted_) {
        is_bidirectionally_sorted_ = is_bidirectionally_sorted(std::cbegin(elements_), std::cend(elements_));
    }
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::iterator
MappableSet<MappableType, Allocator>::insert(std::initializer_list<MappableType> il)
{
    max_element_size_ = std::max(max_element_size_, ::size(*largest_element(il.begin(), il.end())));
    return elements_.insert(std::move(il));
    if (is_bidirectionally_sorted_) {
        is_bidirectionally_sorted_ = is_bidirectionally_sorted(std::cbegin(elements_), std::cend(elements_));
    }
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::iterator
MappableSet<MappableType, Allocator>::erase(const_iterator p)
{
    if (max_element_size_ == ::size(*p)) {
        auto it = elements_.erase(p);
        max_element_size_ = ::size(*largest_element(std::cbegin(elements_), std::cend(elements_)));
        return it;
    }
    if (!is_bidirectionally_sorted_) {
        is_bidirectionally_sorted_ = is_bidirectionally_sorted(std::cbegin(elements_), std::cend(elements_));
    }
    return elements_.erase(p);
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::iterator
MappableSet<MappableType, Allocator>::erase(const MappableType& m)
{
    if (max_element_size_ == ::size(m)) {
        auto it = elements_.erase(m);
        max_element_size_ = ::size(*largest_element(std::cbegin(elements_), std::cend(elements_)));
        return it;
    }
    if (!is_bidirectionally_sorted_) {
        is_bidirectionally_sorted_ = is_bidirectionally_sorted(std::cbegin(elements_), std::cend(elements_));
    }
    return elements_.erase(m);
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::iterator
MappableSet<MappableType, Allocator>::erase(const_iterator first, const_iterator last)
{
    if (max_element_size_ == ::size(*largest_element(first, last))) {
        auto it = elements_.erase(first, last);
        max_element_size_ = ::size(*largest_element(std::cbegin(elements_), std::cend(elements_)));
        return it;
    }
    if (!is_bidirectionally_sorted_) {
        is_bidirectionally_sorted_ = is_bidirectionally_sorted(std::cbegin(elements_), std::cend(elements_));
    }
    return elements_.erase(first, last);
}

//template <typename MappableType, typename Allocator>
//void
//MappableSet<MappableType, Allocator>::erase(OverlapRange<const_iterator> overlapped)
//{
//    bool update_max_element_size {max_element_size_ == ::size(*largest_element(overlapped.begin(), overlapped.end()))};
//    
//    if (is_bidirectionally_sorted_) {
//        elements_.erase(overlapped.begin().base(), overlapped.end().base());
//    } else {
//        // we must be careful not to invalidate iterators
//        
//        
//        
//        is_bidirectionally_sorted_ = is_bidirectionally_sorted(std::cbegin(elements_), std::cend(elements_));
//    }
//    
//    if (update_max_element_size) {
//        max_element_size_ = ::size(*largest_element(std::cbegin(elements_), std::cend(elements_)));
//    }
//}

template <typename MappableType, typename Allocator>
void MappableSet<MappableType, Allocator>::clear()
{
    elements_.clear();
    max_element_size_ = 0;
    is_bidirectionally_sorted_ = true;
}

template <typename MappableType, typename Allocator>
void MappableSet<MappableType, Allocator>::swap(const MappableSet& m)
{
    elements_.swap(m.elements_);
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::size() const noexcept
{
    return elements_.size();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::capacity() const noexcept
{
    return elements_.capacity();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::max_size() const noexcept
{
    return elements_.max_size();
}

template <typename MappableType, typename Allocator>
bool MappableSet<MappableType, Allocator>::empty() const noexcept
{
    return elements_.empty();
}

template <typename MappableType, typename Allocator>
void
MappableSet<MappableType, Allocator>::reserve(size_type n)
{
    elements_.reserve(n);
}

template <typename MappableType, typename Allocator>
void
MappableSet<MappableType, Allocator>::shrink_to_fit()
{
    elements_.shrink_to_fit();
}

template <typename MappableType, typename Allocator>
typename MappableSet<MappableType, Allocator>::allocator_type
MappableSet<MappableType, Allocator>::get_allocator()
{
    return elements_.get_allocator();
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
bool
MappableSet<MappableType, Allocator>::has_overlapped(const MappableType_& mappable) const
{
    return (is_bidirectionally_sorted_) ?
    ::has_overlapped(std::begin(elements_), std::end(elements_), mappable, MappableRangeOrder::BidirectionallySorted)
    :
    ::has_overlapped(std::begin(elements_), std::end(elements_), mappable);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
bool
MappableSet<MappableType, Allocator>::has_overlapped(iterator first, iterator last, const MappableType_& mappable) const
{
    return (is_bidirectionally_sorted_) ?
    ::has_overlapped(first, last, mappable, MappableRangeOrder::BidirectionallySorted)
    :
    ::has_overlapped(first, last, mappable, max_element_size_);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
bool
MappableSet<MappableType, Allocator>::has_overlapped(const_iterator first, const_iterator last, const MappableType_& mappable) const
{
    return (is_bidirectionally_sorted_) ?
    ::has_overlapped(first, last, mappable, MappableRangeOrder::BidirectionallySorted)
    :
    ::has_overlapped(first, last, mappable, max_element_size_);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::count_overlapped(const MappableType_& mappable) const
{
    auto overlapped = overlap_range(mappable);
    return (is_bidirectionally_sorted_) ? ::size(overlapped, MappableRangeOrder::BidirectionallySorted) : ::size(overlapped);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::count_overlapped(iterator first, iterator last, const MappableType_& mappable) const
{
    auto overlapped = overlap_range(first, last, mappable);
    return (is_bidirectionally_sorted_) ? ::size(overlapped, MappableRangeOrder::BidirectionallySorted) : ::size(overlapped);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::count_overlapped(const_iterator first, const_iterator last, const MappableType_& mappable) const
{
    auto overlapped = overlap_range(first, last, mappable);
    return (is_bidirectionally_sorted_) ? ::size(overlapped, MappableRangeOrder::BidirectionallySorted) : ::size(overlapped);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
OverlapRange<typename MappableSet<MappableType, Allocator>::const_iterator>
MappableSet<MappableType, Allocator>::overlap_range(const MappableType_& mappable) const
{
    return overlap_range(std::cbegin(elements_), std::cend(elements_), mappable);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
OverlapRange<typename MappableSet<MappableType, Allocator>::iterator>
MappableSet<MappableType, Allocator>::overlap_range(iterator first, iterator last, const MappableType_& mappable) const
{
    return overlap_range(const_iterator(first), const_iterator(last), mappable);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
OverlapRange<typename MappableSet<MappableType, Allocator>::const_iterator>
MappableSet<MappableType, Allocator>::overlap_range(const_iterator first, const_iterator last, const MappableType_& mappable) const
{
    return (is_bidirectionally_sorted_) ?
        ::overlap_range(first, last, mappable, MappableRangeOrder::BidirectionallySorted)
    :
        ::overlap_range(first, last, mappable, max_element_size_);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
bool
MappableSet<MappableType, Allocator>::has_contained(const MappableType_& mappable) const
{
    return has_contained(std::cbegin(elements_), std::cend(elements_), mappable);
    
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
bool
MappableSet<MappableType, Allocator>::has_contained(iterator first, iterator last, const MappableType_& mappable) const
{
    return has_contained(const_iterator(first), const_iterator(last), mappable);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
bool
MappableSet<MappableType, Allocator>::has_contained(const_iterator first, const_iterator last, const MappableType_& mappable) const
{
    return ::has_contained(first, last, mappable);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::count_contained(const MappableType_& mappable) const
{
    return count_contained(std::cbegin(elements_), std::cend(elements_), mappable);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::count_contained(iterator first, iterator last, const MappableType_& mappable) const
{
    return count_contained(const_iterator(first), const_iterator(last), mappable);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::count_contained(const_iterator first, const_iterator last, const MappableType_& mappable) const
{
    auto contained = contained_range(first, last, mappable);
    return (is_bidirectionally_sorted_) ? ::size(contained, MappableRangeOrder::BidirectionallySorted) : ::size(contained);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
ContainedRange<typename MappableSet<MappableType, Allocator>::const_iterator>
MappableSet<MappableType, Allocator>::contained_range(const MappableType_& mappable) const
{
    return contained_range(std::cbegin(elements_), std::cend(elements_), mappable);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
ContainedRange<typename MappableSet<MappableType, Allocator>::iterator>
MappableSet<MappableType, Allocator>::contained_range(iterator first, iterator last, const MappableType_& mappable) const
{
    return contained_range(const_iterator(first), const_iterator(last), mappable);
}

template <typename MappableType, typename Allocator>
template <typename MappableType_>
ContainedRange<typename MappableSet<MappableType, Allocator>::const_iterator>
MappableSet<MappableType, Allocator>::contained_range(const_iterator first, const_iterator last, const MappableType_& mappable) const
{
    return ::contained_range(first, last, mappable);
}

template <typename MappableType, typename Allocator>
template <typename MappableType1_, typename MappableType2_>
bool
MappableSet<MappableType, Allocator>::has_shared(const MappableType1_& mappable1, const MappableType2_& mappable2) const
{
    return has_shared(std::cbegin(elements_), std::cend(elements_), mappable1, mappable2);
}

template <typename MappableType, typename Allocator>
template <typename MappableType1_, typename MappableType2_>
bool
MappableSet<MappableType, Allocator>::has_shared(iterator first, iterator last,
                                                 const MappableType1_& mappable1, const MappableType2_& mappable2) const
{
    return has_shared(const_iterator(first), const_iterator(last), mappable1, mappable2);
}

template <typename MappableType, typename Allocator>
template <typename MappableType1_, typename MappableType2_>
bool
MappableSet<MappableType, Allocator>::has_shared(const_iterator first, const_iterator last,
                                                 const MappableType1_& mappable1, const MappableType2_& mappable2) const
{
    if (inner_distance(mappable1, mappable2) > max_element_size_) return false;
    
    auto m = std::minmax(get_region(mappable1), get_region(mappable2));
    
    auto overlapped_lhs = overlap_range(first, last, m.first);
    
    return std::any_of(overlapped_lhs.begin(), overlapped_lhs.end(),
                       [&m] (const auto& region) {
                           return overlaps(region, m.second);
                       });
}

template <typename MappableType, typename Allocator>
template <typename MappableType1_, typename MappableType2_>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::count_shared(const MappableType1_& mappable1, const MappableType2_& mappable2) const
{
    return count_shared(std::cbegin(elements_), std::cend(elements_), mappable1, mappable2);
}

template <typename MappableType, typename Allocator>
template <typename MappableType1_, typename MappableType2_>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::count_shared(iterator first, iterator last,
                                                   const MappableType1_& mappable1, const MappableType2_& mappable2) const
{
    return count_shared(const_iterator(first), const_iterator(last), mappable1, mappable2);
}

template <typename MappableType, typename Allocator>
template <typename MappableType1_, typename MappableType2_>
typename MappableSet<MappableType, Allocator>::size_type
MappableSet<MappableType, Allocator>::count_shared(const_iterator first, const_iterator last,
                                                   const MappableType1_& mappable1, const MappableType2_& mappable2) const
{
    if (inner_distance(mappable1, mappable2) > max_element_size_) return 0;
    
    auto m = std::minmax(get_region(mappable1), get_region(mappable2));
    
    auto overlapped_lhs = overlap_range(first, last, m.first);
    
    return std::count_if(overlapped_lhs.begin(), overlapped_lhs.end(),
                       [&m] (const auto& region) {
                           return overlaps(region, m.second);
                       });
}

template <typename MappableType, typename Allocator>
template <typename MappableType1_, typename MappableType2_>
SharedRange<typename MappableSet<MappableType, Allocator>::const_iterator>
MappableSet<MappableType, Allocator>::shared_range(const MappableType1_& mappable1, const MappableType2_& mappable2) const
{
    return shared_range(std::cbegin(elements_), std::cend(elements_), mappable1, mappable2);
}

template <typename MappableType, typename Allocator>
template <typename MappableType1_, typename MappableType2_>
SharedRange<typename MappableSet<MappableType, Allocator>::iterator>
MappableSet<MappableType, Allocator>::shared_range(iterator first, iterator last,
                                                   const MappableType1_& mappable1, const MappableType2_& mappable2) const
{
    return shared_range(const_iterator(first), const_iterator(last), mappable1, mappable2);
}

template <typename MappableType, typename Allocator>
template <typename MappableType1_, typename MappableType2_>
SharedRange<typename MappableSet<MappableType, Allocator>::const_iterator>
MappableSet<MappableType, Allocator>::shared_range(const_iterator first, const_iterator last,
                                                   const MappableType1_& mappable1, const MappableType2_& mappable2) const
{
    if (inner_distance(mappable1, mappable2) > max_element_size_) {
        return make_shared_range(last, last, mappable1, mappable2);
    }
    
    auto m = std::minmax(get_region(mappable1), get_region(mappable2));
    
    auto overlapped_lhs = overlap_range(first, last, m.first);
    
    auto it = std::find_if(overlapped_lhs.begin(), overlapped_lhs.end(),
                           [&m] (const auto& region) {
                               return overlaps(region, m.second);
                           });
    
    auto end = std::prev(overlapped_lhs.end());
    
    while (end != it && !overlaps(*end, m.second)) --end;
    
    return make_shared_range(it.base(), std::next(end).base(), mappable1, mappable2);
}

// non-member methods

template <typename ForwardIterator, typename MappableType1, typename MappableType2>
ForwardIterator
find_first_shared(const MappableSet<MappableType1>& mappables, ForwardIterator first, ForwardIterator last,
                  const MappableType2& mappable)
{
    return std::find_if(first, last,
                        [&mappables, &mappable] (const auto& m) {
                            return mappables.has_shared(m, mappable);
                        });
}

#endif