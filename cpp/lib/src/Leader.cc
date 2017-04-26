/** \file   RegexMatcher.cc
 *  \brief  Implementation of the MARC-21 Leader class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "Leader.h"
#include "Compiler.h"
#include <cctype>
#include <cstdio>
#include "StringUtil.h"


Leader &Leader::operator=(const Leader &rhs) {
    if (likely(&rhs != this)) {
        raw_leader_           =  rhs.raw_leader_;
        record_length_        = rhs.record_length_;
        base_address_of_data_ = rhs.base_address_of_data_;
    }

    return *this;
}


bool Leader::ParseLeader(const std::string &leader_string, Leader * const leader, std::string * const err_msg) {
    if (err_msg != nullptr)
        err_msg->clear();

    if (leader_string.size() != LEADER_LENGTH) {
        if (err_msg != nullptr)
            *err_msg = "Leader length must be " + std::to_string(LEADER_LENGTH) +
                ", found " + std::to_string(leader_string.size()) + "!";
        return false;
    }

    unsigned record_length;
    if (std::sscanf(leader_string.substr(0, 5).data(), "%5u", &record_length) != 1) {
        if (err_msg != nullptr)
            *err_msg = "Can't parse record length! (Found \"" + StringUtil::CStyleEscape(leader_string.substr(0, 5))
                       + "\")";
        return false;
    }

    unsigned base_address_of_data;
    if (std::sscanf(leader_string.substr(12, 5).data(), "%5u", &base_address_of_data) != 1) {
        if (err_msg != nullptr)
            *err_msg = "Can't parse base address of data!";
        return false;
    }

    //
    // Validity checks:
    //

    // Check indicator count:
    if (leader_string[10] != '2') {
        if (err_msg != nullptr)
            *err_msg = "Invalid indicator count '" + leader_string.substr(10, 1) + "'!";
        return false;
    }
  
    // Check subfield code length:
    if (leader_string[11] != '2') {
        if (err_msg != nullptr)
            *err_msg = "Invalid subfield code length!";
        return false;
    }

    // Check entry map:
    if (leader_string.substr(20, 3) != "450") {
        if (err_msg != nullptr)
            *err_msg = "Invalid entry map!";
        return false;
    }

    leader->raw_leader_           = leader_string;
    leader->record_length_        = record_length;
    leader->base_address_of_data_ = base_address_of_data;

    return true;
}


bool Leader::setRecordLength(const unsigned new_record_length, std::string * const err_msg) {
    if (err_msg != nullptr)
        err_msg->clear();

    if (new_record_length < LEADER_LENGTH + 2 /*Directory terminator + Record terminator*/) {
        if (err_msg != nullptr)
            *err_msg = "new record length (" + std::to_string(new_record_length)
                       + ") cannot be less than the length of a leader plus the directory terminator and record terminator!";
        return false;
    }

    record_length_ = new_record_length;
    raw_leader_ = StringUtil::PadLeading(std::to_string(record_length_), 5, '0') + raw_leader_.substr(5);
    return true;
}


void Leader::setBaseAddressOfData(const unsigned new_base_address_of_data) {
    base_address_of_data_ = new_base_address_of_data;
    raw_leader_ = raw_leader_.substr(0, 12) + StringUtil::PadLeading(std::to_string(base_address_of_data_), 5, '0')
                  + raw_leader_.substr(17);
}


Leader::RecordType Leader::getRecordType() const {
    if (raw_leader_[6] == 'z')
        return AUTHORITY;
    if (raw_leader_[6] == 'w')
        return CLASSIFICATION;
    return std::strchr("acdefgijkmoprt", raw_leader_[6]) == nullptr ? UNKNOWN : BIBLIOGRAPHIC;
}
