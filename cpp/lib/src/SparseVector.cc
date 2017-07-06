/** \file   SparseVector.h
 *  \brief  A class for sparse vectors of doubles.
 *  \author Wagner Truppel
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
 *  Copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <SparseVector.h>
#ifndef VECTOR
#       include <vector>
#       define VECTOR
#endif
#ifndef MATH_H
#	include <math.h>
#	define MATH_H
#endif
#ifndef MATH_UTIL_H
#       include <MathUtil.h>
#endif
#ifndef STRING_UTIL_H
#       include <StringUtil.h>
#endif


std::string SparseVector::toString() const {
    std::string result("[ " + StringUtil::ToString(logical_size_) + " | "
                       + StringUtil::ToString(getNumOfNonZeroElements()) + " | ");
    for (IndexValueMap::const_iterator index_value_pair = index_value_pairs_.begin();
         index_value_pair != index_value_pairs_.end(); ++index_value_pair)
    {
        if (index_value_pair != index_value_pairs_.begin())
            result += ", ";

        result += "(" + StringUtil::ToString(index_value_pair->first)  + ", ";

        if (index_value_pair->second >= 0)
            result += " ";

        result += StringUtil::ToString(index_value_pair->second) + ")";
    }
    result += " ]";

    return result;
}


const SparseVector &SparseVector::operator=(const SparseVector &rhs) {
    if (likely(&rhs != this)) {
        logical_size_ = rhs.logical_size_;
        index_value_pairs_ = rhs.index_value_pairs_;
    }

    return *this;
}


void SparseVector::set(const unsigned i, const real value) {
    if (unlikely(i >= logical_size_))
        throw std::runtime_error("in SparseVector::set: Index out of bounds!");

	// The first branch of this if block looks strange, but it's needed because some matrix operations might
	// store intermediate values for the i-th element which later end up adding to zero, in which case we want
	// to remove its storage.

	if (unlikely(value == 0.0))
		index_value_pairs_.erase(i);
	else
		index_value_pairs_[i] = value;
}


real SparseVector::magnitude() const {
    real total(0.0);
    for (IndexValueMap::const_iterator index_value_pair = index_value_pairs_.begin();
         index_value_pair != index_value_pairs_.end(); ++index_value_pair)
        total += index_value_pair->second * index_value_pair->second;

    return sqrt(total);
}


real SparseVector::operator[](const unsigned i) const {
    if (unlikely(i >= logical_size_))
        throw std::runtime_error("in SparseVector::operator[]: Index out of bounds!");

    IndexValueMap::const_iterator index_value_pair = index_value_pairs_.find(i);
    return index_value_pair == index_value_pairs_.end() ? 0.0 : index_value_pair->second;
}


const SparseVector &SparseVector::operator+=(const SparseVector &rhs)
{
    if (unlikely(this->size() != rhs.size()))
        throw std::runtime_error("size of 'this' vector (" + StringUtil::ToString(this->size()) +
                                 ") does not match size of argument vector rhs (" + StringUtil::ToString(rhs.size())
                                 + ").");

    // Iterate through rhs's non-zero elements, adding them to this vector. If the result for a given
    // element is zero, we must remove that element.
    for (IndexValueMap::const_iterator index_value_pair = rhs.index_value_pairs_.begin();
         index_value_pair != rhs.index_value_pairs_.end(); ++index_value_pair)
    {
        this->index_value_pairs_[index_value_pair->first] += index_value_pair->second;
        if (unlikely(this->index_value_pairs_[index_value_pair->first] == 0.0))
            index_value_pairs_.erase(index_value_pair->first);
    }

    return *this;
}


const SparseVector &SparseVector::operator-=(const SparseVector &rhs) {
    if (unlikely(this->size() != rhs.size()))
        throw std::runtime_error("size of 'this' vector (" + StringUtil::ToString(this->size()) +
                                 ") does not match size of argument vector rhs (" + StringUtil::ToString(rhs.size())
                                 + ").");

    // Iterate through rhs's non-zero elements, subtracting them from this vector. If the result for a given
    // element is zero, we must remove that element.
    for (IndexValueMap::const_iterator index_value_pair = rhs.index_value_pairs_.begin();
         index_value_pair != rhs.index_value_pairs_.end(); ++index_value_pair)
    {
        this->index_value_pairs_[index_value_pair->first] -= index_value_pair->second;
        if (unlikely(this->index_value_pairs_[index_value_pair->first] == 0.0))
            index_value_pairs_.erase(index_value_pair->first);
    }

    return *this;
}


const SparseVector &SparseVector::operator*=(const real s) {
    if (unlikely(s == 0.0)) {
        index_value_pairs_.clear();
        return *this;
    }

    // Iterate through this vector's non-zero elements, multiplying them by s.
    for (IndexValueMap::iterator index_value_pair = index_value_pairs_.begin();
         index_value_pair != index_value_pairs_.end(); ++index_value_pair)
        index_value_pair->second *= s;

    return *this;
}


const SparseVector &SparseVector::operator/=(const real s) {
	if (unlikely(s == 0.0))
            throw std::runtime_error("in SparseVector::operator/=: cannot divide by zero!");

	// Iterate through this vector's non-zero elements, dividing them by s.
	for (IndexValueMap::iterator index_value_pair = index_value_pairs_.begin();
	     index_value_pair != index_value_pairs_.end(); ++index_value_pair)
            index_value_pair->second /= s;

	return *this;
}


SparseVector SparseVector::operator+(const SparseVector &v) const {
    SparseVector tmp(*this);
    tmp += v;
    return tmp;
}


SparseVector SparseVector::operator-(const SparseVector &v) const {
    SparseVector tmp(*this);
    tmp -= v;
    return tmp;
}


SparseVector SparseVector::operator*(const real s) const {
    SparseVector tmp(*this);
    tmp *= s;
    return tmp;
}


SparseVector SparseVector::operator/(const real s) const {
    SparseVector tmp(*this);
    tmp /= s;
    return tmp;
}


real SparseVector::operator*(const SparseVector &v) const {
    if (unlikely(this->size() != v.size()))
        throw std::runtime_error("in SparseVector::operator*: size of 'this' vector ("
                                 + StringUtil::ToString(this->size()) + ") does not match size of argument vector v ("
                                 + StringUtil::ToString(v.size()) + ").");

    // We want to iterate through the vector with the smallest number of non-zero elements.
    if (this->getNumOfNonZeroElements() < v.getNumOfNonZeroElements()) {
        // Iterate through this vector's elements.
        return SparseVector::DotProd(*this, v);
    } else // Iterate through v's elements.
        return SparseVector::DotProd(v, *this);
}


real SparseVector::DotProd(const SparseVector &u, const SparseVector &v) {
    // No point in wasting cycles multiplying against zero!
    if (unlikely(u.getNumOfNonZeroElements() == 0) or unlikely(v.getNumOfNonZeroElements() == 0))
        return 0.0;

    // Keep intermediate non-zero summands of the form u[i] * v[i].
    // The number of non-zero summands is at most the smallest of u.getNumOfNonZeroElements() and
    // v.getNumOfNonZeroElements(). But that's guaranteed to be u.getNumOfNonZeroElements().
    MathUtil::NumericallySafeSum<real> dot_prod_elems(u.getNumOfNonZeroElements());

    for (IndexValueMap::const_iterator index_value_pair(u.index_value_pairs_.begin());
         index_value_pair != u.index_value_pairs_.end(); ++index_value_pair)
    {
        const real v_value = v[index_value_pair->first];
        if (likely(v_value != 0.0))
            dot_prod_elems += index_value_pair->second * v_value;
    }

    return dot_prod_elems.sum();
}


SparseVector operator*(const real s, const SparseVector &v) {
    SparseVector tmp(v);
    tmp *= s;
    return tmp;
}
