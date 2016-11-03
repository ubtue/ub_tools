/** \file   DirectoryEntry.cc
 *  \brief  Implementation of the DirectoryEntry class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014,2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "DirectoryEntry.h"
#include "Compiler.h"
#include "StringUtil.h"
#include "util.h"


const size_t DirectoryEntry::DIRECTORY_ENTRY_LENGTH(12);
const size_t DirectoryEntry::TAG_LENGTH(3);


DirectoryEntry::DirectoryEntry(const std::string &raw_entry) {
    if (raw_entry.size() != DIRECTORY_ENTRY_LENGTH)
        Error("incorrect raw directory entry size (" + std::to_string(raw_entry.size()) + ").  Must be 12!");
    tag_ = raw_entry.substr(0, TAG_LENGTH);

    if (std::sscanf(raw_entry.data() + TAG_LENGTH, "%4u", &field_length_) != 1)
        Error("can't scan field length (" + raw_entry.substr(TAG_LENGTH, 4)
              + ") in directory entry! (Tag was " + tag_.to_string() + ")");

    if (std::sscanf(raw_entry.data() + 7, "%5u", &field_offset_) != 1)
        Error("can't scan field oddset in directory entry!");
}


DirectoryEntry &DirectoryEntry::operator=(const DirectoryEntry &rhs) {
    if (likely(&rhs != this)) {
        tag_          = rhs.tag_;
        field_length_ = rhs.field_length_;
        field_offset_ = rhs.field_offset_;
    }

    return *this;
}


std::string DirectoryEntry::toString() const {
    std::string field_as_string;
    field_as_string.reserve(DirectoryEntry::DIRECTORY_ENTRY_LENGTH);

    field_as_string += tag_.to_string();
    field_as_string += StringUtil::PadLeading(std::to_string(field_length_), 4, '0');
    field_as_string += StringUtil::PadLeading(std::to_string(field_offset_), 5, '0');

    return field_as_string;
}


bool DirectoryEntry::ParseDirEntries(const std::string &entries_string, std::vector<DirectoryEntry> * const entries,
                                     std::string * const err_msg) {
    entries->clear();
    if ((entries_string.length() % DIRECTORY_ENTRY_LENGTH) != 1) {
        if (err_msg != nullptr)
            *err_msg = "Raw directory entries string must be a multiple of " + std::to_string(DIRECTORY_ENTRY_LENGTH)
                       + " in length plus 1 for the record separator at the end of the directory! "
                       "Length found was " + std::to_string(entries_string.length()) + ".)";
        return false;
    }

    if (entries_string[entries_string.length() - 1] != '\x1E') {
        if (err_msg != nullptr)
            *err_msg = "Missing field terminator at end of directory!";
        return false;
    }

    const unsigned count(entries_string.length() / DIRECTORY_ENTRY_LENGTH);
    entries->reserve(count);
    size_t offset(0);
    for (unsigned i(0); i < count; ++i) {
        entries->emplace_back(DirectoryEntry(entries_string.substr(offset, DIRECTORY_ENTRY_LENGTH)));
        offset += DIRECTORY_ENTRY_LENGTH;
    }

    return true;
}
