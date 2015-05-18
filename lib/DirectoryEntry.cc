/** \file   DirectoryEntry.cc
 *  \brief  Implementation of the DirectoryEntry class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014 Universitätsbiblothek Tübingen.  All rights reserved.
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
	      + ") in directory entry! (Tag was " + tag_ + ")");

    if (std::sscanf(raw_entry.data() + 7, "%5u", &field_offset_) != 1)
	Error("can't scan field oddset in directory entry!");
}


std::string DirectoryEntry::toString() const {
    std::string field_as_string;
    field_as_string.reserve(DirectoryEntry::DIRECTORY_ENTRY_LENGTH);

    field_as_string += tag_;
    field_as_string += StringUtil::PadLeading(std::to_string(field_length_), 4, '0');
    field_as_string += StringUtil::PadLeading(std::to_string(field_offset_), 5, '0');

    return field_as_string;
}


bool DirectoryEntry::ParseDirEntries(const std::string &entries_string, std::vector<DirectoryEntry> * const entries,
				     std::string * const err_msg) {
    entries->clear();
    if ((entries_string.length() % DIRECTORY_ENTRY_LENGTH) != 1) {
	if (err_msg != NULL)
	    *err_msg = "Raw directory entries string must be a multiple of " + std::to_string(DIRECTORY_ENTRY_LENGTH)
		+ " in length!";
	return false;
    }

    if (entries_string[entries_string.length() - 1] != '\x1E') {
	if (err_msg != NULL)
	    *err_msg = "Missing field terminator at end of directory!";
	return false;
    }

    const unsigned count(entries_string.length() / DIRECTORY_ENTRY_LENGTH);
    entries->reserve(count);
    size_t offset(0);
    for (unsigned i(0); i < count; ++i) {
	entries->push_back(DirectoryEntry(entries_string.substr(offset, DIRECTORY_ENTRY_LENGTH)));
	offset += DIRECTORY_ENTRY_LENGTH;
    }

    return true;
}


class MatchTag {
    const std::string tag_to_match_;
public:
    explicit MatchTag(const std::string &tag_to_match): tag_to_match_(tag_to_match) { }
    inline bool operator()(const DirectoryEntry &entry_to_compare_against) const {
	return entry_to_compare_against.getTag() == tag_to_match_;
    }
};


std::vector<DirectoryEntry>::const_iterator DirectoryEntry::FindField(
    const std::string &tag, const std::vector<DirectoryEntry> &field_entries) 
{
    return std::find_if(field_entries.begin(), field_entries.end(), MatchTag(tag));
}


std::pair<std::vector<DirectoryEntry>::const_iterator, std::vector<DirectoryEntry>::const_iterator>
    DirectoryEntry::FindFields(const std::string &tag, const std::vector<DirectoryEntry> &field_entries)
{
    std::pair<std::vector<DirectoryEntry>::const_iterator, std::vector<DirectoryEntry>::const_iterator> retval;
    retval.first = FindField(tag, field_entries);
    if (retval.first == field_entries.end())
	retval.second = field_entries.end();
    else {
	retval.second = retval.first;
	retval.second++;
	while (retval.second != field_entries.end() and retval.second->getTag() == tag)
	    ++retval.second;
    }

    return retval;
}

