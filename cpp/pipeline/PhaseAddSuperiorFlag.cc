/** \file    PhaseAddSuperiorFlag.cc
 *  \brief   A tool for marking superior records that have associated inferior records in our data sets.
 *  \author  Oliver Obenland
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

#include "PhaseAddSuperiorFlag.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "File.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "MarcXmlWriter.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include <exception>


static unsigned modified_count(0);
static std::set <std::string> superior_ppns;
static std::string superior_subfield_data;


PipelinePhaseState PhaseAddSuperiorFlag::preprocess(const MarcUtil::Record &record, std::string *const) {
    monitor->startTiming("PhaseAddSuperiorFlag", __FUNCTION__);

    const std::string subfieldTags[4] = {"800", "810", "830", "773"};
    for (auto subfieldTag : subfieldTags) {
        std::vector <std::string> subfields;
        record.extractSubfields(subfieldTag, "w", &subfields);

        for (auto subfield : subfields) {
            if (StringUtil::StartsWith(subfield, "(DE-576)"))
                superior_ppns.insert(subfield.substr(8));
        }
    }

    return SUCCESS;
};


PipelinePhaseState PhaseAddSuperiorFlag::process(MarcUtil::Record &record, std::string *const error_message) {
    auto messure(monitor->startTiming("PhaseAddSuperiorFlag", __FUNCTION__));

    // Don't add the flag twice
    if (record.getFieldIndex("SPR") != -1) {
        return SUCCESS;
    }

    const std::vector <std::string> &field_data(record.getFields());
    const auto iter(superior_ppns.find(field_data.at(0)));
    if (iter != superior_ppns.end()) {
        if (not record.insertField("SPR", superior_subfield_data)) {
            return MakeError("Not enough room to add a SPR field! (Control number: " + field_data[0] + ")",
                             error_message);
        }

        ++modified_count;
    }

    return SUCCESS;
};


PhaseAddSuperiorFlag::PhaseAddSuperiorFlag() {
    Subfields superior_subfield(/* indicator1 = */' ', /* indicator2 = */' ');
    superior_subfield.addSubfield('a', "1"); // Could be anything but we can't have an empty field.
    superior_subfield_data = superior_subfield.toString();
}


PhaseAddSuperiorFlag::~PhaseAddSuperiorFlag() {
    monitor->setCounter("PhaseAddSuperiorFlag", "superior ppns", superior_ppns.size());
    monitor->setCounter("PhaseAddSuperiorFlag", "modified", modified_count);
}