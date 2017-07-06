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

#ifndef SPARSE_VECTOR_H
#define SPARSE_VECTOR_H


#include <unordered_map>
#include <Real.h>


/** \class SparseVector */
class SparseVector {
    /** \brief   The vector's logical size. */
    unsigned logical_size_;

    /** \brief   The actual storage for the (non-zero) vector elements. */
    typedef std::unordered_map<unsigned, real> IndexValueMap;
    IndexValueMap index_value_pairs_;

public:
    class const_iterator {
        friend class SparseVector;
        IndexValueMap::const_iterator map_iter_;
    public:
        const_iterator(const const_iterator &rhs): map_iter_(rhs.map_iter_) { }
        unsigned getIndex() const { return map_iter_->first; }
        real getValue() const { return map_iter_->second; }
        void operator++() { ++map_iter_; }
        bool operator!=(const const_iterator &rhs) const { return map_iter_ != rhs.map_iter_; }
    private:
        explicit const_iterator(const IndexValueMap::const_iterator &rhs): map_iter_(rhs) { }
    };

public:
    /** \brief   Constructs a vector with the given logical size where all elements are
     *           initialized to zero.
     *  \param   initial_size  The vector's desired logical size.
     */
    explicit SparseVector(const unsigned initial_size = 0) : logical_size_(initial_size) { }

    /** \brief   The copy constructor. */
    SparseVector(const SparseVector &rhs)
        : logical_size_(rhs.logical_size_), index_value_pairs_(rhs.index_value_pairs_) { }

    /** \brief  Returns the logical size of this vector. */
    unsigned size() const { return logical_size_; }

    /** \brief  Changes the logical size of this vector. Note that this function _does_ reset all elements to
     *          their "zero" values (the vector is, then, as sparse as it can be).
     */
    void resize(const unsigned new_size) {
        index_value_pairs_.clear();
        logical_size_ = new_size;
    }

    /** \brief  Clears the contents of this vector, making it as sparse as it can be. Notice that this function
     *          does _not_ reset the vector's logical size to zero (use resize(0) to achieve both tasks).
     */
    void resetToZero() { index_value_pairs_.clear(); }

    /** \brief  Returns the number of non-zero elements of this vector. */
    unsigned getNumOfNonZeroElements() const { return static_cast<unsigned>(index_value_pairs_.size()); }

    /** \brief  Returns a (standard) string representation of this vector. */
    std::string toString() const;

    /** \brief  Sets this vector to be an identical copy of the argument vector rhs. */
    const SparseVector &operator=(const SparseVector &rhs);

    /** \brief  Sets the i-th element of this vector to the desired value. Indices are zero-based. */
    void set(const unsigned i, const real value);

    /** \brief  Gets the manitude of the vector based on SQRT(SUM(this[0]**2, this[1]**2, ... this[size_-1]**2)) */
    real magnitude() const;

    /** \brief  Returns the i-th element of this vector as an R-value only. Indices are zero-based. */
    real operator[](const unsigned i) const;

    /** \brief  Adds the input vector rhs to this vector.
     *          Throws an exception if the two vectors do not have the same logical size.
     */
    const SparseVector &operator+=(const SparseVector &rhs);

    /** \brief  Subtracts the input vector rhs from this vector.
     *          Throws an exception if the two vectors do not have the same logical size.
     */
    const SparseVector &operator-=(const SparseVector &rhs);

    /** \brief  Multiplies this vector by the input scalar s. */
    const SparseVector &operator*=(const real s);

    /** \brief  Divides this vector by the input scalar s.
     *          Throws an exception if s is zero.
     */
    const SparseVector &operator/=(const real s);

    /** \brief  Returns a new vector, equal to the vector addition (this + v). */
    SparseVector operator+(const SparseVector &v) const;

    /** \brief  Returns a new vector, equal to the vector difference (this - v). */
    SparseVector operator-(const SparseVector &v) const;

    /** \brief  Returns a new vector, equal to this vector multiplied by the scalar s. */
    SparseVector operator*(const real s) const;

    /** \brief  Returns a new vector, equal to this vector divided by the scalar s.
     *          Throws an exception if the scalar s is zero.
     */
    SparseVector operator/(const real s) const;

    /** \brief  Returns the inner product (dot product) between this vector and the input vector v.
     *          The calculation is robust against overflows. Throws an exception if the two vectors
     *          do not have the same logical size.
     */
    real operator*(const SparseVector &v) const;

    const_iterator begin() const { return const_iterator(index_value_pairs_.begin()); }

    const_iterator end() const { return const_iterator(index_value_pairs_.end()); }

private:
    /** \brief  Returns the inner product (dot product) between two vectors, iterating through the elements
     *          of the first vector. The calculation is robust against overflows.
     */
    static real DotProd(const SparseVector &u, const SparseVector &v);
};


/** \brief  Returns a new vector, equal to the product s * v. */
SparseVector operator*(const real s, const SparseVector &v);


#endif // ifndef SPARSE_VECTOR_H
