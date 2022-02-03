/** \brief Utility functions for pairs of unsigneds.
 *  \author Dr. Johannes Ruscheinski
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once


typedef std::pair<unsigned, unsigned> UnsignedPair;


inline UnsignedPair operator+(const UnsignedPair &rhs, const UnsignedPair &lhs) {
    return UnsignedPair(rhs.first + lhs.first, rhs.second + lhs.second);
}


inline UnsignedPair &operator+=(UnsignedPair &rhs, const UnsignedPair &lhs) {
    rhs.first += lhs.first, rhs.second += lhs.second;
    return rhs;
}
