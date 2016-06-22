/** \file    PhaseAugment773a.cc
 *  \brief   A tool for filling in 773$a if the 773 field exists and $a is missing.
 *  \author  Dr. Johannes Ruscheinski
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

#include "PhaseAugment773a.h"

#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"

static std::unordered_map <std::string, std::string> control_numbers_to_titles_map;
static unsigned patch_count;


PipelinePhaseState PhaseAugment773a::preprocess(const MarcUtil::Record &record, std::string * const) {
    ssize_t _245_index;
    if (likely((_245_index = record.getFieldIndex("245")) != -1)) {
        const std::vector <std::string> &fields(record.getFields());
        const Subfields _245_subfields(fields[_245_index]);
        std::string title(_245_subfields.getFirstSubfieldValue('a'));
        if (_245_subfields.hasSubfield('b'))
            title += " " + _245_subfields.getFirstSubfieldValue('b');
        StringUtil::RightTrim(" \t/", &title);
        if (likely(not title.empty()))
            control_numbers_to_titles_map[record.getControlNumber()] = title;
    }
    return SUCCESS;
};


PipelinePhaseState PhaseAugment773a::process(MarcUtil::Record &record, std::string * const) {
    ssize_t _773_index;
    if ((_773_index = record.getFieldIndex("773")) != -1) {
        const std::vector <std::string> &fields(record.getFields());
        const Subfields _773_subfields(fields[_773_index]);
        if (not _773_subfields.hasSubfield('a') and _773_subfields.hasSubfield('w')) {
            const std::string w_subfield(_773_subfields.getFirstSubfieldValue('w'));
            if (StringUtil::StartsWith(w_subfield, "(DE-576)")) {
                const std::string parent_control_number(w_subfield.substr(8));
                const auto control_number_and_title(control_numbers_to_titles_map.find(parent_control_number));
                if (control_number_and_title != control_numbers_to_titles_map.end()) {
                    record.updateField(_773_index, fields[_773_index] + "\x1F""a" + control_number_and_title->second);
                    ++patch_count;
                }
            }
        }
    }
    return SUCCESS;
};


PhaseAugment773a::~PhaseAugment773a() {
    std::cerr << "Augment 773a:\n";
    std::cerr << "\tFound " << control_numbers_to_titles_map.size() << " control number to title mappings.\n";
    std::cerr << "\tAdded 773$a subfields to " << patch_count << " records.\n";
}