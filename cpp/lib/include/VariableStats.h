/** \file    VariableStats.h
 *  \brief   Implementation of class VariableStats.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Wagner Truppel
 */

/*
 *  Copyright 2004-2009 Project iVia.
 *  Copyright 2004-2009 The Regents of The University of California.
 *  Copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef VARIABLE_STATS_H
#define VARIABLE_STATS_H


#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include <Compiler.h>
#include <StringUtil.h>


/** \class  VariableStats
 *  \brief  Calculates the mean and standard deviation of a set of observations.
 */
template<typename NumericType> class VariableStats {
    NumericType total_;
    unsigned count_;
public:
    enum VariableStatsType {
        WITH_STANDARD_DEVIATION,      ///< Allows the computation of standard deviations.
        WITHOUT_STANDARD_DEVIATION    ///< Does not allow the computation of standard deviations.
    };
private:
    VariableStatsType stats_type_;
    std::vector<NumericType> values_;
public:
    explicit VariableStats(const VariableStatsType stats_type = WITHOUT_STANDARD_DEVIATION,
                           const NumericType &total = 0, const unsigned count = 0)
        : total_(total), count_(count), stats_type_(stats_type) { }

    VariableStats(const VariableStats<NumericType> &rhs)
        : total_(rhs.total_), count_(rhs.count_), stats_type_(rhs.stats_type_), values_(rhs.values_) { }

    const VariableStats<NumericType> &operator=(const VariableStats<NumericType> &rhs);

    void operator+=(const VariableStats<NumericType> &rhs);

    void operator+=(const NumericType &value);

    void accrue(const NumericType &value, const unsigned count = 1);

    void clear();

    double mean() const;

    double standardDeviation() const;

    std::string toString(const unsigned no_decimal_digits = 2) const;

    unsigned getCount() const { return count_; }

    NumericType getTotal() const { return total_; }

    bool canComputeStandardDeviation() const { return stats_type_ == WITH_STANDARD_DEVIATION; }
};


typedef VariableStats<unsigned long> TimeInMillisecsStats; // For convenience when measuring time...


template<typename NumericType> const VariableStats<NumericType> &VariableStats<NumericType>::operator=(
    const VariableStats<NumericType> &rhs)
{
    if (likely(this != &rhs)) {
        total_ = rhs.total_;
        count_ = rhs.count_;
        stats_type_ = rhs.stats_type_;
        values_ = rhs.values_;
    }

    return *this;
}


template<typename NumericType> void VariableStats<NumericType>::operator+=(const VariableStats<NumericType> &rhs) {
    if (unlikely(stats_type_ != rhs.stats_type_))
        throw std::runtime_error("in VariableStats::operator+=: instances of different stat-types!");

    if (stats_type_ == WITH_STANDARD_DEVIATION) {
        for (typename std::vector<NumericType>::const_iterator entry(rhs.values_.begin());
             entry != rhs.values_.end(); ++entry)
            values_.push_back(*entry);
    }

    total_ += rhs.total_;
    count_ += rhs.count_;
}


template<typename NumericType> void VariableStats<NumericType>::operator+=(const NumericType &value) {
    if (stats_type_ == WITH_STANDARD_DEVIATION)
        values_.push_back(value);

    total_ += value;
    ++count_;
}


template<typename NumericType> void VariableStats<NumericType>::accrue(const NumericType &value, const unsigned count)
{
    if (stats_type_ == WITH_STANDARD_DEVIATION)
        values_.push_back(value);

    total_ += value;
    count_ += count;
}


template<typename NumericType> void VariableStats<NumericType>::clear() {
    total_ = 0;
    count_ = 0;
    values_.clear();
}


template<typename NumericType> double VariableStats<NumericType>::mean() const {
    if (unlikely(count_ == 0)) {
        if (likely(total_ == 0))
            return 0.0;
        else
            throw std::runtime_error("in VariableStats::mean: unexpected division by 0!");
    } else
        return static_cast<double>(total_) / static_cast<double>(count_);
}


template<typename NumericType> double VariableStats<NumericType>::standardDeviation() const {
    if (unlikely(stats_type_ == WITHOUT_STANDARD_DEVIATION))
        throw std::runtime_error("in VariableStats::standardDeviation: this instantiation does not support the "
                                 "computation of standard deviation!");

    if (unlikely(count_ == 0)) {
        if (likely(values_.empty()))
            return 0.0;
        else
            throw std::runtime_error("in VariableStats::standardDeviation: unexpected division by 0!");
    }

    double avg = mean();
    double std_dev_squared = 0.0;
    for (typename std::vector<NumericType>::const_iterator entry(values_.begin()); entry != values_.end(); ++entry) {
        const double temp = static_cast<double>(*entry) - avg;
        std_dev_squared += temp * temp;
    }
    std_dev_squared /= static_cast<double>(count_); // NOT values_.size() !!!
    // (that would be incorrect because count_ can be set
    //  independently of the size of the vector.)

    return std::sqrt(std_dev_squared);
}


template<typename NumericType> std::string VariableStats<NumericType>::toString(const unsigned no_decimal_digits)
    const
{
    std::string result(StringUtil::ToString(mean(), no_decimal_digits));
    if (stats_type_ == WITH_STANDARD_DEVIATION)
        result += " +/- " + StringUtil::ToString(standardDeviation(), no_decimal_digits);
    return result;
}


#endif // ifndef VARIABLE_STATS_H
