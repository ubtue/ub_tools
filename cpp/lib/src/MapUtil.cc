/** \file   MapUtil.cc
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
#include "MapUtil.h"
#include "File.h"
#include "StringUtil.h"


namespace MapUtil {


/** \brief Replaces slashes, equal-signs and semicolons with a slash followed by the respective character. */
std::string Escape(const std::string &s) {
    std::string retval;
    retval.reserve(s.length());

    for (const char ch : s) {
        if (ch == '\\' or ch == '=' or ch == ';')
            retval += '\\';
        retval += ch;
    }

    return retval;
}


void DeserialiseMap(const std::string &input_filename, std::unordered_map<std::string, std::string> * const map,
                    const bool revert_keys_and_values) {
    map->clear();

    std::ifstream input(input_filename, std::ofstream::in);
    if (input.fail())
        LOG_ERROR("Failed to open \"" + input_filename + "\" for reading!");

    unsigned line_no(0);
    for (std::string line; std::getline(input, line); /* Intentionally empty! */) {
        ++line_no;

        // Deal w/ comments, leading and trailing spaces and empty lines:
        const size_t last_hash_pos(line.find_last_of('#'));
        if (last_hash_pos != std::string::npos)
            line = line.substr(0, last_hash_pos);
        StringUtil::Trim(&line);
        if (line.empty())
            continue;

        std::string key, value;
        bool in_key(true), escaped(false);
        for (const char ch : line) {
            if (escaped) {
                escaped = false;
                if (in_key)
                    key += ch;
                else
                    value += ch;
            } else if (ch == '\\')
                escaped = true;
            else if (ch == '=') {
                if (key.empty())
                    LOG_ERROR("Missing key in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
                else if (not in_key)
                    LOG_ERROR("Unescaped equal-sign in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
                in_key = false;
            } else if (in_key)
                key += ch;
            else
                value += ch;
        }
        if (key.empty() or value.empty())
            LOG_ERROR("Bad input in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
        if (revert_keys_and_values)
            key.swap(value);
        if (unlikely(map->find(key) != map->end()))
            LOG_ERROR("Duplicate key in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
        (*map)[key] = value;
    }
}


void SerialiseMap(const std::string &output_filename, const std::unordered_map<std::string, std::string> &map) {
    std::ofstream output(output_filename, std::ofstream::out | std::ofstream::trunc);
    if (output.fail())
        LOG_ERROR("Failed to open \"" + output_filename + "\" for writing!");

    for (const auto &key_and_value : map)
        output << Escape(key_and_value.first) << '=' << Escape(key_and_value.second) << '\n';
}


void SerialiseMap(const std::string &output_filename, const std::unordered_multimap<std::string, std::string> &multimap) {
    std::ofstream output(output_filename, std::ofstream::out | std::ofstream::trunc);
    if (output.fail())
        LOG_ERROR("Failed to open \"" + output_filename + "\" for writing!");

    for (const auto &key_and_value : multimap)
        output << Escape(key_and_value.first) << '=' << Escape(key_and_value.second) << '\n';
}


void DeserialiseMap(const std::string &input_filename, std::unordered_multimap<std::string, std::string> * const multimap) {
    multimap->clear();

    std::ifstream input(input_filename, std::ofstream::in);
    if (input.fail())
        LOG_ERROR("Failed to open \"" + input_filename + "\" for reading!");

    unsigned line_no(0);
    for (std::string line; std::getline(input, line); /* Intentionally empty! */) {
        ++line_no;

        // Deal w/ comments, leading and trailing spaces and empty lines:
        const size_t last_hash_pos(line.find_last_of('#'));
        if (last_hash_pos != std::string::npos)
            line = line.substr(0, last_hash_pos);
        StringUtil::Trim(&line);
        if (line.empty())
            continue;

        std::string key, value;
        bool in_key(true), escaped(false);
        for (const char ch : line) {
            if (escaped) {
                escaped = false;
                if (in_key)
                    key += ch;
                else
                    value += ch;
            } else if (ch == '\\')
                escaped = true;
            else if (ch == '=') {
                if (key.empty())
                    LOG_ERROR("Missing key in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
                else if (not in_key)
                    LOG_ERROR("Unescaped equal-sign in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
                in_key = false;
            } else if (in_key)
                key += ch;
            else
                value += ch;
        }
        if (key.empty() or value.empty())
            LOG_ERROR("Bad input in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
        multimap->emplace(key, value);
    }
}


void WriteEntry(File * const map_file, const std::string &key, const std::string &value) {
    map_file->writeln(Escape(key) + "=" + Escape(value));
}


} // namespace MapUtil
