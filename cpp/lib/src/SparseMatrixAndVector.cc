/** \file    SparseMatrixAndVector.cc
 *  \brief   Implementation of the SparseMatrix and Vector classes.
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

#include <SparseMatrixAndVector.h>
#include <iomanip>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <climits>
#include <cstdint>
#include <cmath>
#include <Compiler.h>
#include <Logger.h>


SparseMatrix::SparseMatrix(const size_type n): size_(n), default_value_(0.0) {
    if (n > UINT_MAX)
        throw std::runtime_error("in SparseMatrix::SparseMatrix: initial size " + std::to_string(n)
                                 + " exceeds " + std::to_string(UINT_MAX) + " ( = max. allowed size)!");
}


namespace {


bool RowLessThan(const IndicesAndValue &lhs, const IndicesAndValue &rhs) {
    return lhs.row_ < rhs.row_;
}


} // unnamed namespace


void SparseMatrix::rowSort() const {
    SparseMatrix * const non_const_this(const_cast<SparseMatrix *>(this));
    std::sort(non_const_this->begin(), non_const_this->end(), RowLessThan);
    rehash();
}


namespace {


bool ColLessThan(const IndicesAndValue &lhs, const IndicesAndValue &rhs) {
    return lhs.col_ < rhs.col_;
}


} // unnamed namespace


void SparseMatrix::colSort() const {
    SparseMatrix * const non_const_this(const_cast<SparseMatrix *>(this));
    std::sort(non_const_this->begin(), non_const_this->end(), ColLessThan);
    rehash();
}


void SparseMatrix::clear() {
    row_and_column_to_vector_index_map_.clear();
    size_ = 0;
    rehash();
}


void SparseMatrix::transpose() {
    for (iterator iter(begin()); iter != end(); ++iter)
        iter->swapRowAndColumn();
}


SparseMatrix SparseMatrix::transposedCopy() {
    SparseMatrix new_matrix(*this);
    new_matrix.transpose();
    return new_matrix;
}


void SparseMatrix::multiplySelfBySelfT() {
    *this *= (this->transposedCopy());
}



namespace {


bool RowAndColLessThan(const IndicesAndValue &lhs, const IndicesAndValue &rhs) {
    if (lhs.row_ == rhs.row_)
        return lhs.col_ < rhs.col_;
    else
        return lhs.row_ < rhs.row_;
}


} // unnamed namespace


void SparseMatrix::setValue(const unsigned row, const unsigned col, const double new_value) {
    if (row > size_)
        throw std::range_error("in SparseMatrix::setValue: row index out of range!");
    if (col > size_)
        throw std::range_error("in SparseMatrix::setValue: column index out of range!");

    uint64_t row_and_column(row);
    row_and_column <<= 32u;
    row_and_column |= col;
    std::unordered_map<uint64_t, unsigned>::const_iterator iter(row_and_column_to_vector_index_map_.find(row_and_column));
    if (iter != row_and_column_to_vector_index_map_.end()) { // We already have an entry => replace the value.
        //		if (new_value == 0.0)
        //			row_and_column_to_vector_index_map_.erase(iter);
        //		else
        (*this)[iter->second].value_ = new_value;
    } else if (new_value != default_value_) { // We need to create a new entry.
        const unsigned new_index(std::vector<IndicesAndValue>::size());
        row_and_column_to_vector_index_map_.insert(std::make_pair(row_and_column, new_index));
        std::vector<IndicesAndValue>::push_back(IndicesAndValue(row, col, new_value));
    }
}


double SparseMatrix::getValue(const unsigned row, const unsigned col) const {
    if (row > size_)
        throw std::range_error("in SparseMatrix::getValue: row index out of range!");
    if (col > size_)
        throw std::range_error("in SparseMatrix::getValue: column index out of range!");

    uint64_t row_and_column(row);
    row_and_column <<= 32u;
    row_and_column |= col;
    std::unordered_map<uint64_t, unsigned>::const_iterator iter(row_and_column_to_vector_index_map_.find(row_and_column));
    if (iter != row_and_column_to_vector_index_map_.end())
        return (*this)[iter->second].value_;
    else // We don't have an explicit entry for this matrix element and therefore assume it is the default value:
        return default_value_;
}


SparseVector SparseMatrix::getRow(const unsigned row) const {
    SparseVector vector_to_return(size_);
    for (unsigned col(0); col < size_; ++col) {
        const double col_val(this->getValue(row, col));

        // SparseMatrix has a mutable default value; SparseVector
        // can only handle 0.0 as a default.
        if (col_val != 0.0)
            vector_to_return.set(col, col_val);
    }

    return vector_to_return;
}


SparseVector SparseMatrix::getCol(const unsigned col) const {
    SparseVector vector_to_return(size_);
    for (unsigned row(0); row < size_; ++row) {
        const double row_val(this->getValue(row, col));

        // SparseMatrix has a mutable default value; SparseVector
        // can only handle 0.0 as a default.
        if (row_val != 0.0)
            vector_to_return.set(row, row_val);
    }

    return vector_to_return;
}


void SparseMatrix::print(std::ostream &output, const int precision) const {
    std::vector<IndicesAndValue> sorted(*this);
    std::sort(sorted.begin(), sorted.end(), RowAndColLessThan);

    std::vector<IndicesAndValue>::const_iterator indices_and_value(sorted.begin());
    for (size_type row = 0; row < size(); ++row) {
        output << "(";
        for (size_type col = 0; col < size(); ++col) {
            if (col != 0)
                output << ", ";
            if (indices_and_value != sorted.end() and indices_and_value->row_ == row
                and indices_and_value->col_ == col)
                {
                    output << std::setprecision(precision) << indices_and_value->value_;
                    ++indices_and_value;
                }
            else
                output << std::setprecision(precision) << default_value_;
        }
        output << ")\n";
    }
}


void SparseMatrix::printMatlabRepresentation(std::ostream &output, const int precision) const {
    std::vector<IndicesAndValue> sorted(*this);
    std::sort(sorted.begin(), sorted.end(), RowAndColLessThan);

    output << '[';
    std::vector<IndicesAndValue>::const_iterator indices_and_value(sorted.begin());
    for (size_type row = 0; row < size(); ++row) {
        output << ' ';
        for (size_type col = 0; col < size(); ++col) {
            if (col != 0)
                output << ' ';
            if (indices_and_value != sorted.end() and indices_and_value->row_ == row
                and indices_and_value->col_ == col)
                {
                    output << std::setprecision(precision) << indices_and_value->value_;
                    ++indices_and_value;
                }
            else
                output << std::setprecision(precision) << default_value_;
        }
        if (row != size() - 1)
            output << ";\n";
    }
    output << "]\n";
}


void SparseMatrix::log(const std::string &log_message, Logger * const logger, const int precision) const {
    std::stringstream str_stream;

    std::vector<IndicesAndValue> sorted(*this);
    std::sort(sorted.begin(), sorted.end(), RowAndColLessThan);

    std::vector<IndicesAndValue>::const_iterator indices_and_value(sorted.begin());
    for (size_type row = 0; row < size(); ++row) {
        str_stream << "\t(";
        for (size_type col = 0; col < size(); ++col) {
            if (col != 0)
                str_stream << ", ";
            if (indices_and_value != sorted.end() and indices_and_value->row_ == row
                and indices_and_value->col_ == col)
            {
                str_stream << std::setprecision(precision) << indices_and_value->value_;
                ++indices_and_value;
            } else
                str_stream << std::setprecision(precision) << default_value_;
        }
        str_stream << ")\n";
    }

    logger->log(log_message + "\n" + str_stream.str());
}


void SparseMatrix::rehash() const {
    std::unordered_map<uint64_t, unsigned> * const non_const_row_and_column_to_vector_index_map(
        const_cast<std::unordered_map<uint64_t, unsigned> *>(&row_and_column_to_vector_index_map_));
    non_const_row_and_column_to_vector_index_map->clear();
    for (unsigned index(0); index < size(); ++index) {
        uint64_t row_and_column((*this)[index].row_);
        row_and_column <<= 32u;
        row_and_column |= (*this)[index].col_;
        non_const_row_and_column_to_vector_index_map->insert(std::make_pair(row_and_column, index));
    }
}


SparseMatrix &SparseMatrix::operator*=(const SparseMatrix &b) {
    size_t a_size(size_), b_size(b.size_);
    std::vector<SparseVector> a_rows(a_size);
    for (unsigned row(0); row < a_size; ++row)
        a_rows[row] = getRow(row);
    clear();
    std::vector<SparseVector> b_cols(b_size);
    for (unsigned col(0); col < b_size; ++col)
        b_cols[col] = b.getCol(col);
    for (unsigned a_row_counter(0); a_row_counter < a_size; ++a_row_counter) {
        for (unsigned b_col_counter(0); b_col_counter < b_size; ++b_col_counter) {
            double sum(0);
            for(SparseVector::const_iterator a_value = a_rows[a_row_counter].begin();
                a_value != a_rows[a_row_counter].end(); ++a_value)
            {
                for(SparseVector::const_iterator b_value = b_cols[b_col_counter].begin();
                    b_value != b_cols[b_col_counter].end(); ++b_value)
                    sum += a_value.getValue() * b_value.getValue();
            }
            setValue(b_col_counter, a_row_counter, sum);
        }
    }

    return *this;
}


SparseMatrix &SparseMatrix::operator*=(const SparseVector &vector_to_transpose) {
    SparseMatrix matrix_to_multiply_by;
    for (SparseVector::const_iterator indices(vector_to_transpose.begin()); indices != vector_to_transpose.end();
         ++indices)
        matrix_to_multiply_by.setValue(indices.getIndex(), 0, indices.getValue());
    this->operator*=(matrix_to_multiply_by);

    return *this;
}


const Vector &Vector::operator+=(const Vector &rhs) {
    // Make sure the two vectors are compatible:
    if (unlikely(size() != rhs.size()))
        throw std::runtime_error("in Vector::operator+=(const Vector &rhs): \"rhs\" and current vector "
                                 "have incompatible sizes!");

    for (size_type i = 0; i < size(); ++i)
        (*this)[i] += rhs[i];

    return *this;
}


const Vector &Vector::operator*=(const double &rhs) {
    for (size_type i = 0; i < size(); ++i)
        (*this)[i] *= rhs;

    return *this;
}


const Vector &Vector::operator*=(const SparseMatrix &rhs) {
    // Make sure that this vector and the sparse matrix are compatible:
    if (unlikely(size() != rhs.size()))
        throw std::runtime_error("in &Vector::operator*=(const SparseMatrix &rhs): \"rhs\" and current vector "
                                 "have incompatible sizes!");

    Vector tmp(size(), 0.0);

    const double rhs_default_value(rhs.getDefaultValue());
    if (rhs_default_value == 0.0) {
        for (SparseMatrix::const_iterator indices_and_value(rhs.begin()); indices_and_value != rhs.end();
             ++indices_and_value)
            tmp[indices_and_value->col_] += (*this)[indices_and_value->row_] * indices_and_value->value_;
    } else { // Matrix "rhs" has a non-zero default value.
        for (unsigned col = 0; col < size(); ++col) {
            for (unsigned row = 0; row < size(); ++row)
                tmp[col] += (*this)[row] * rhs.getValue(row, col);
        }
    }

    return *this = tmp;
}


void Vector::l1Normalise() {
    double norm(0.0);
    for (unsigned i = 0; i < size(); ++i)
        norm += std::fabs((*this)[i]);

    if (norm == 0.0)
        std::runtime_error("in Vector::l1Normalise: norm must not be zero!");

    for (unsigned i = 0; i < size(); ++i)
        (*this)[i] /= norm;
}


void Vector::print(std::ostream &output, const int precision) const
{
	output << "(";
	for (size_t i = 0; i < size(); ++i) {
		if (i != 0)
			output << ", ";
		output << std::setprecision(precision) << (*this)[i];
	}
	output << ")";
}


void Vector::log(const std::string &log_message, Logger * const logger, const int precision) const
{
	std::stringstream str_stream;
	print(str_stream, precision);
	logger->log(log_message + " " + str_stream.str());
}
