/** \file    SparseMatrixAndVector.h
 *  \brief   Declaration of classes SparseMatrix and Vector.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Jason Scheirer
 */

/*
 *  Copyright 2005-2007 Project iVia.
 *  Copyright 2005-2007 The Regents of The University of California.
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

#ifndef SPARSE_MATRIX_AND_VECTOR_H
#define SPARSE_MATRIX_AND_VECTOR_H


#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include <inttypes.h>
#include <SparseVector.h>


// Forward declaration(s):
class Logger;


struct IndicesAndValue {
    unsigned row_, col_;
    double value_;
public:
    IndicesAndValue(const unsigned row, const unsigned col, const double value)
        : row_(row), col_(col), value_(value) { }
    void swapRowAndColumn() { std::swap(row_, col_); }
};


/** \class  SparseMatrix
 *  \brief  Implements an efficient sparse square-matrix with entries of type "double".
 */
class SparseMatrix: private std::vector<IndicesAndValue> {
    std::unordered_map<uint64_t, unsigned> row_and_column_to_vector_index_map_;
public:
    typedef std::vector<IndicesAndValue>::size_type size_type;
    typedef std::vector<IndicesAndValue>::const_iterator const_iterator;
    typedef std::vector<IndicesAndValue>::iterator iterator;
private:
    size_type size_;
    double default_value_;
public:
    explicit SparseMatrix(const size_type n = 0);

    size_type size() const { return size_; }
    bool empty() const { return size_ == 0; }

    void setDefaultValue(const double new_default_value) { default_value_ = new_default_value; }
    double getDefaultValue() const { return default_value_; }

    /** Transposes self */
    void transpose();

    /** Returns transposed copy */
    SparseMatrix transposedCopy();

    /** Multiplies self by transposed copy */
    void multiplySelfBySelfT();

    /** Sorts elements based on increasing row indices. */
    void rowSort() const;

    /** Sorts elements based on increasing column indices. */
    void colSort() const;

    /** Clear all entries */
    void clear();

    using std::vector<IndicesAndValue>::begin;
    using std::vector<IndicesAndValue>::end;

    void setValue(const unsigned row, const unsigned col, const double new_value);
    double getValue(const unsigned row, const unsigned col) const;

    /** \brief Gets a sparse vector for a row of the matrix, where the indices are column values
     */
    SparseVector getRow(const unsigned row) const;

    /** \brief Gets a sparse vector for a column of the matrix, where the indices are row values
     */
    SparseVector getCol(const unsigned col) const;

    /** \brief Displays the matrix on "output" using "precision" digits for the elements.
        Sorts elements as a side effect. */
    void print(std::ostream &output, const int precision) const;

    /** \brief Displays the matrix on "output" using "precision" digits for the elements.
        Sorts elements as a side effect. */
    void printMatlabRepresentation(std::ostream &output, const int precision) const;

    void log(const std::string &log_message, Logger * const logger, const int precision = 3) const;

    SparseMatrix &operator*=(const SparseMatrix &);

    /** \brief Multiply a matrix by a transposed sparse vector (Nx1 SparseVector flipped to a 1xN SparseMatrix) */
    SparseMatrix &operator*=(const SparseVector &);
private:
    /** Update the mapping from the rows and columns of non-default elements to their indices. */
    void rehash() const;
};


class Vector: public std::vector<double> {
public:
    typedef std::vector<double>::size_type size_type;
public:
    Vector(): std::vector<double>() { }
    explicit Vector(const size_type initial_size, const double &initial_value = double())
        : std::vector<double>(initial_size, initial_value) { }
    const Vector &operator+=(const Vector &rhs);
    const Vector &operator*=(const double &rhs);
    const Vector &operator*=(const SparseMatrix &rhs);
    void l1Normalise();
    void print(std::ostream &output, const int precision) const;
    void log(const std::string &log_message, Logger * const logger, const int precision = 3) const;
};


#endif // ifndef SPARSE_MATRIX_AND_VECTOR_H
