/** \file   MapUtil.h
 *  \brief  Map-Util-related utility functions.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <fstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include "StringUtil.h"
#include "util.h"


// Forward declaration:
class File;


namespace MapUtil {


/** \brief Replaces slashes, equal-signs and semicolons with a slash followed by the respective character. */
std::string Escape(const std::string &s);


template <typename KeyType, typename ValueType>
inline bool Contains(const std::unordered_multimap<KeyType, ValueType> &multimap, const KeyType &key, const ValueType &value) {
    const auto range(multimap.equal_range(key));
    for (auto key_and_value(range.first); key_and_value != range.second; ++key_and_value) {
        if (key_and_value->second == value)
            return true;
    }
    return false;
}


/** \brief Writes "map" to "output_filename" in a format that can be red in by DeserialiseMap(). */
template <typename MapType>
void SerialiseMap(const std::string &output_filename, const MapType &map) {
    std::ofstream output(output_filename, std::ofstream::out | std::ofstream::trunc);
    if (output.fail())
        LOG_ERROR("Failed to open \"" + output_filename + "\" for writing!");

    for (const auto &key_and_value : map)
        output << Escape(AnyToString(key_and_value.first)) << '=' << AnyToString(key_and_value.second) << '\n';
}


/** \brief Writes "map" to "output_filename" in a format that can be red in by DeserialiseMap(). */
void SerialiseMap(const std::string &output_filename, const std::unordered_map<std::string, std::string> &map);


/** \brief Reads "map: from "input_filename".  Aborts on input errors and emits an error message on stderr.
 *  \note  Optional hash signs start comments that extend to the end of a line.
 */
void DeserialiseMap(const std::string &input_filename, std::unordered_map<std::string, std::string> * const map,
                    const bool revert_keys_and_values = false);

/** \brief Writes "multimap" to "output_filename" in a format that can be red in by DeserialiseMap(). */
void SerialiseMap(const std::string &output_filename, const std::unordered_multimap<std::string, std::string> &map);

/** \brief Reads "multimap" from "input_filename".  Aborts on input errors and emits an error message on stderr. */
void DeserialiseMap(const std::string &input_filename, std::unordered_multimap<std::string, std::string> * const multimap);

void WriteEntry(File * const map_file, const std::string &key, const std::string &value);


// Useful for debugging..
template <typename MapType>
std::string MapToString(const MapType &map) {
    std::string map_as_string;

    for (const auto &key_and_value : map) {
        if (not map_as_string.empty())
            map_as_string += ", ";
        map_as_string += '"';
        map_as_string += StringUtil::BackslashEscape('"', StringUtil::AnyToString(key_and_value.first));
        map_as_string += "\" = \"";
        map_as_string += StringUtil::BackslashEscape('"', StringUtil::AnyToString(key_and_value.second));
        map_as_string += '"';
    }

    return map_as_string;
}


} // namespace MapUtil
