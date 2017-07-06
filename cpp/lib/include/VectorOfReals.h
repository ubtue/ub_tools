/** \file   VectorOfReals.h
 *  \brief  A class for dense vectors of doubles.
 *  \author Wagner Truppel
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
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


#ifndef VECTOR_OF_REALS_H
#define VECTOR_OF_REALS_H


#include <Real.h>
#include <string>
#include <vector>


/** \class VectorOfReals */
class VectorOfReals {
    std::vector<real> vector_;
public:
    /** \brief   Constructs a vector with the given size where all elements are
     *           initialized to zero.
     *  \param   initial_size  The vector's desired size.
     */
    explicit VectorOfReals(const unsigned initial_size = 0): vector_(initial_size) { }

    size_t size() const { return vector_.size(); }
    void resize(const size_t new_size) { return vector_.resize(new_size); }

    /** \brief  Returns a (standard) string representation of this vector. */
    std::string toString() const;

    /** \brief  Sets a random number of vector components to random values in the range [min, max]. */
    void randomize(const real min, const real max);

    /** \brief  Clears the contents of this vector. Notice that this function does _not_ reset the vector's
     *	    logical size to zero.
     */
    void resetToZero();

    /** \brief  Adds the input vector rhs to this vector.
     *          Throws an exception if the two vectors do not have the same logical size.
     */
    const VectorOfReals &operator+=(const VectorOfReals &rhs);

    /** \brief  Subtracts the input vector rhs from this vector.
     *          Throws an exception if the two vectors do not have the same logical size.
     */
    const VectorOfReals &operator-=(const VectorOfReals &rhs);

    /** \brief  Multiplies this vector by the input scalar s. */
    const VectorOfReals &operator*=(const real s);

    /** \brief  Divides this vector by the input scalar s. Throws an exception if s is zero. */
    const VectorOfReals &operator/=(const real s);

    /** \brief  Returns a new vector, equal to the vector addition (this + v). */
    VectorOfReals operator+(const VectorOfReals &v) const;

    /** \brief  Returns a new vector, equal to the vector difference (this - v). */
    VectorOfReals operator-(const VectorOfReals &v) const;

    /** \brief  Returns a new vector, equal to this vector multiplied by the scalar s. */
    VectorOfReals operator*(const real s) const;

    /** \brief  Returns a new vector, equal to this vector divided by the scalar s.
     *          Throws an exception if the scalar s is zero.
     */
    VectorOfReals operator/(const real s) const;

    /** \brief  Returns the inner product (dot product) between this vector and the input vector v.
     *          The calculation is robust against overflows and minimizes round-off errors. Throws
     *          an exception if the two vectors do not have the same logical size.
     */
    real operator*(const VectorOfReals &v) const;

    const real &operator[](const size_t index) const { return vector_[index]; }
    real &operator[](const size_t index) { return vector_[index]; }
};


/** \brief  Returns a new vector, equal to the product s * v. */
VectorOfReals operator*(const real s, const VectorOfReals &v);


#endif // ifndef VECTOR_OF_REALS_H
