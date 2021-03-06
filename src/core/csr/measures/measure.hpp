// Copyright (c) 2017 Daniel Cooke
// Use of this source code is governed by the MIT license that can be found in the LICENSE file.

#ifndef measure_hpp
#define measure_hpp

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <unordered_map>

#include <boost/variant.hpp>
#include <boost/optional.hpp>
#include <boost/any.hpp>

#include "../facets/facet.hpp"

namespace octopus {

class VcfRecord;

namespace csr {

class Measure
{
public:
    using FacetMap = std::unordered_map<std::string, FacetWrapper>;
    using ResultType = boost::variant<double, boost::optional<double>,
                                      std::size_t, boost::optional<std::size_t>,
                                      bool,
                                      boost::any>;
    
    Measure() = default;
    
    Measure(const Measure&)            = default;
    Measure& operator=(const Measure&) = default;
    Measure(Measure&&)                 = default;
    Measure& operator=(Measure&&)      = default;
    
    virtual ~Measure() = default;
    
    std::unique_ptr<Measure> clone() const { return do_clone(); }
    
    ResultType evaluate(const VcfRecord& call, const FacetMap& facets) const { return do_evaluate(call, facets); }
    std::string name() const { return do_name(); }
    std::vector<std::string> requirements() const { return do_requirements(); }
    std::string serialise(const ResultType& value) const { return do_serialise(value); }
    
private:
    virtual std::unique_ptr<Measure> do_clone() const = 0;
    virtual ResultType do_evaluate(const VcfRecord& call, const FacetMap& facets) const = 0;
    virtual std::string do_name() const = 0;
    virtual std::vector<std::string> do_requirements() const { return {}; }
    virtual std::string do_serialise(const ResultType& value) const;
};

class MeasureWrapper
{
public:
    MeasureWrapper() = delete;
    
    MeasureWrapper(std::unique_ptr<Measure> measure) : measure_ {std::move(measure)} {}
    
    MeasureWrapper(const MeasureWrapper& other) : measure_ {other.measure_->clone()} {}
    MeasureWrapper& operator=(const MeasureWrapper& other)
    {
        if (&other != this) measure_ = other.measure_->clone();
        return *this;
    }
    MeasureWrapper(MeasureWrapper&&)            = default;
    MeasureWrapper& operator=(MeasureWrapper&&) = default;
    
    ~MeasureWrapper() = default;
    
    const Measure* base() const noexcept { return measure_.get(); }
    auto operator()(const VcfRecord& call) const { return measure_->evaluate(call, {}); }
    auto operator()(const VcfRecord& call, const Measure::FacetMap& facets) const { return measure_->evaluate(call, facets); }
    std::string name() const { return measure_->name(); }
    std::vector<std::string> requirements() const { return measure_->requirements(); }
    std::string serialise(const Measure::ResultType& value) const { return measure_->serialise(value); }
    
private:
    std::unique_ptr<Measure> measure_;
};

template <typename M, typename... Args>
MeasureWrapper make_wrapped_measure(Args&&... args)
{
    return MeasureWrapper {std::make_unique<M>(std::forward<Args>(args)...)};
}

template <typename Measure>
std::string name()
{
    return Measure().name();
}

namespace detail {

struct IsMissingMeasureVisitor : public boost::static_visitor<bool>
{
    template <typename T> bool operator()(const boost::optional<T>& value) const noexcept { return !value; }
    template <typename T> bool operator()(const T& value) const noexcept { return false; }
};

} // namespace detail

inline bool is_missing(const Measure::ResultType& value) noexcept
{
    return boost::apply_visitor(detail::IsMissingMeasureVisitor {}, value);
}

} // namespace csr
} // namespace octopus

#endif
