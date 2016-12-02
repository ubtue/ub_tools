/** \file    delete_unused_local_data.cc
 *  \author  Oliver Obenland
 *
 *  Local data blocks are embedded marc records inside of a record using LOK-Fields.
 *  Each local data block belongs to an institution and is marked by the institution's sigil.
 *  This tool filters for local data blocks of some institutions of the University of Tübingen
 *  and deletes all other local blocks.
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MARC_TAG_H
#define MARC_TAG_H


#include <arpa/inet.h>
#include <string>
#include <stdint.h>
#include "util.h"

class MarcTag {
    /* We have to double this up, so we have one little endian integer for comparison, and one big endian integer
     * containing a char[4] for printig.
     */
    union {
        uint32_t as_int_;
        char as_cstring_[4];
    } tag_;
public:
    inline MarcTag() = default;
    inline MarcTag(const char raw_tag[4]) {
        tag_.as_int_ = 0;
        tag_.as_cstring_[0] = raw_tag[0];
        tag_.as_cstring_[1] = raw_tag[1];
        tag_.as_cstring_[2] = raw_tag[2];
    }

    inline MarcTag(const std::string &raw_tag) {
        if (unlikely(raw_tag.length() != 3))
            throw std::runtime_error("in MarcTag: \"raw_tag\" must have a length of 3: " + raw_tag);
        tag_.as_int_ = 0;
        tag_.as_cstring_[0] = raw_tag[0];
        tag_.as_cstring_[1] = raw_tag[1];
        tag_.as_cstring_[2] = raw_tag[2];
    }

    /** Copy constructor. */
    MarcTag(const MarcTag &other_tag): tag_(other_tag.tag_) {}

    bool operator==(const MarcTag &rhs) const { return to_int() == rhs.to_int(); }
    bool operator!=(const MarcTag &rhs) const { return to_int() != rhs.to_int(); }
    bool operator>(const MarcTag &rhs) const  { return to_int() >  rhs.to_int(); }
    bool operator>=(const MarcTag &rhs) const { return to_int() >= rhs.to_int(); }
    bool operator<(const MarcTag &rhs) const  { return to_int() <  rhs.to_int(); }
    bool operator<=(const MarcTag &rhs) const { return to_int() <= rhs.to_int(); }

    bool operator==(const std::string &rhs) const { return ::strcmp(c_str(), rhs.c_str()) == 0; }
    bool operator==(const char rhs[4]) const { return ::strcmp(c_str(), rhs) == 0; }

    std::ostream& operator<<(std::ostream& os) const { return os << to_string(); }
    friend std::ostream &operator<<(std::ostream &output,  const MarcTag &tag) { return output << tag.to_string(); }

    inline const char *c_str() const { return tag_.as_cstring_; }
    inline const std::string to_string() const { return std::string(c_str(), 3); }
    inline uint32_t to_int() const { return htonl(tag_.as_int_); }

    inline bool isTagOfControlField() const { return tag_.as_cstring_[0] == '0' && tag_.as_cstring_[1] == '0'; }
};


namespace std {
    template <>
    struct hash<MarcTag> {
        size_t operator()(const MarcTag &m) const {
            // hash method here.
            return hash<int>()(m.to_int());
        }
    };
}


#endif // MARC_TAG_H
