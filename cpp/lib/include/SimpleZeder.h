/** \brief Very simple class for retrieval of values from any of our Zeder instances.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>


class SimpleZeder {
public:
    enum InstanceType { IXTHEO, KRIM_DOK };
public:
    class Journal {
        friend class SimpleZeder;
        typedef std::unordered_map<std::string, std::string> ShortColumnNameToValuesMap;
        ShortColumnNameToValuesMap short_column_names_to_values_map_;
    public:
        Journal() = default;
        Journal(const Journal &) = default;
        Journal(ShortColumnNameToValuesMap &short_column_names_to_values_map)
            { short_column_names_to_values_map_.swap(short_column_names_to_values_map); }
    public:
        std::string lookup(const std::string &short_column_name) const;
        inline size_t size() const { return short_column_names_to_values_map_.size(); }
        inline bool empty() const { return short_column_names_to_values_map_.empty(); }
    };
private:
    std::vector<Journal> journals_;
public:
    typedef std::vector<Journal>::const_iterator const_iterator;
public:
    // \param "column_filter" If not empty, only the specified short column names will be accesible via the
    //        lookup member function of class Journal.  This is a performance and memory optimisation only.
    explicit SimpleZeder(const InstanceType instance_type, const std::unordered_set<std::string> &column_filter = {});

    inline size_t size() const { return journals_.size(); }
    inline size_t empty() const { return journals_.empty(); }
    inline const_iterator begin() const { return journals_.cbegin(); }
    inline const_iterator end() const { return journals_.cend(); }
};
