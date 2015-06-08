/** \file   Subfields.cc
 *  \brief  Implementation of the Subfields class.
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
#include "Subfields.h"
#include "util.h"


Subfields::Subfields(const std::string &field_data) {
    if (field_data.size() < 3) {
	indicator1_ = indicator2_ = '\0';
	return;
    }

    std::string::const_iterator ch(field_data.begin());
    indicator1_ = *ch++;
    indicator2_ = *ch++;
    
    while (ch != field_data.end()) {
	if (*ch != '\x1F')
	    Error("Expected subfield code delimiter not found!");

	++ch;
	if (ch == field_data.end())
	    Error("Unexpected subfield data end while expecting a subfield code!");
	const char subfield_code = *ch++;

	std::string subfield_data;
	while (ch != field_data.end() and *ch != '\x1F')
	    subfield_data += *ch++;
	if (subfield_data.empty())
	    Error("Empty subfield for code '" + std::to_string(subfield_code) + "'!");

	subfield_code_to_data_map_.insert(std::make_pair(subfield_code, subfield_data));
    }
}


bool Subfields::hasSubfieldWithValue(const char subfield_code, const std::string &value) const {
    for (auto code_and_value(subfield_code_to_data_map_.find(subfield_code));
	 code_and_value != subfield_code_to_data_map_.end() and code_and_value->first == subfield_code;
	 ++code_and_value)
    {
	if (code_and_value->second == value)
	    return true;
    }

    return false;
}


std::string Subfields::getFirstSubfieldValue(const char subfield_code) const {
    const auto begin_end(getIterators(subfield_code));
    if (begin_end.first == begin_end.second)
	return "";
    return begin_end.first->second;
}


void Subfields::replace(const char subfield_code, const std::string &old_value, const std::string &new_value) {
    bool found(false);

    const std::pair<Iterator, Iterator> begin_end(getIterators(subfield_code));
    for (Iterator code_and_value(begin_end.first); code_and_value != begin_end.second; ++code_and_value) {
	if (code_and_value->second == old_value) {
	    found = true;
	    code_and_value->second = new_value;
	}
    }

    if (not found)
	Error("Unexpected: tried to replace \"" + old_value + "\" with \"" + new_value + "\" in subfield '"
	      + subfield_code + "' but did not find the original value!");
}


void Subfields::erase(const char subfield_code, const std::string &value) {
    Iterator code_and_value(subfield_code_to_data_map_.find(subfield_code));

    while (code_and_value != subfield_code_to_data_map_.end() and code_and_value->first == subfield_code) {
	if (code_and_value->second == value)
	    code_and_value = subfield_code_to_data_map_.erase(code_and_value);
	else
	    ++code_and_value;
    }
}


std::string Subfields::toString() const {
    std::string as_string;

    as_string += indicator1_;
    as_string += indicator2_;

    for (ConstIterator code_and_value(subfield_code_to_data_map_.begin());
	 code_and_value != subfield_code_to_data_map_.end(); ++code_and_value)
    {
	as_string += '\x1F';
	as_string += code_and_value->first;
	as_string += code_and_value->second;
    }

    return as_string;
}
