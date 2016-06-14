/** \file    PhaseAddIsbnsOrIssnsToArticles.cc
 *  \brief   A tool for adding missing ISBN's (field 020$a) or ISSN's (field 773$x)to articles entries,
 *           in MARC-21 data.
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

#include "PhaseAddIsbnsOrIssnsToArticles.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


static std::unordered_map <std::string, std::string> parent_id_to_isbn_and_issn_map;
unsigned count_norm_data(0), extracted_isbn_count(0), extracted_issn_count(0);
unsigned count(0), isbns_added(0), issns_added(0), missing_host_record_ctrl_num_count(0), missing_isbn_or_issn_count(0);


bool IsPossibleISSN(const std::string &issn_candidate) {
    static RegexMatcher *matcher(nullptr);
    std::string err_msg;
    if (unlikely(matcher == nullptr)) {
        matcher = RegexMatcher::RegexMatcherFactory("\\d{4}\\-\\d{3}[\\dX]", &err_msg);
        if (matcher == nullptr)
            Error(err_msg);
    }

    const bool is_possible_issn(matcher->matched(issn_candidate, &err_msg));
    if (unlikely(not err_msg.empty()))
        Error(err_msg);

    return is_possible_issn;
}


PipelinePhaseState PhaseAddIsbnsOrIssnsToArticles::preprocess(const MarcUtil::Record &record, std::string * const) {
    const Leader &leader(record.getLeader());
    if (not leader.isSerial())
        return SUCCESS;

    const std::vector <std::string> &fields(record.getFields());
    const std::string isbn(record.extractFirstSubfield("020", 'a'));
    if (not isbn.empty()) {
        parent_id_to_isbn_and_issn_map[fields[0]] = isbn;
        ++extracted_isbn_count;
    }

    const std::string issn(record.extractFirstSubfield("022", 'a'));
    if (not issn.empty()) {
        parent_id_to_isbn_and_issn_map[fields[0]] = issn;
        ++extracted_issn_count;
    }
    return SUCCESS;
};


PipelinePhaseState PhaseAddIsbnsOrIssnsToArticles::process(MarcUtil::Record &record, std::string * const error_message) {
    const Leader &leader(record.getLeader());
    if (not leader.isArticle())
        return SUCCESS;

    const std::vector <DirectoryEntry> &dir_entries(record.getDirEntries());
    if (dir_entries[0].getTag() != "001") {
        return MakeError("First field is not \"001\"!", error_message);
    }

    auto entry_iterator(DirectoryEntry::FindField("773", dir_entries));
    if (entry_iterator == dir_entries.end())
        return SUCCESS;

    const size_t index_773(entry_iterator - dir_entries.begin());
    const std::vector <std::string> &fields(record.getFields());
    Subfields subfields(fields[index_773]);
    if (subfields.hasSubfield('x'))
        return SUCCESS;

    auto begin_end = subfields.getIterators('w'); // Record control number of Host Item Entry.
    if (begin_end.first == begin_end.second) {
        ++missing_host_record_ctrl_num_count;
        return SUCCESS;
    }

    std::string host_id(begin_end.first->second);
    if (StringUtil::StartsWith(host_id, "(DE-576)"))
        host_id = host_id.substr(8);
    auto const parent_isbn_or_issn_iter(parent_id_to_isbn_and_issn_map.find(host_id));
    if (parent_isbn_or_issn_iter == parent_id_to_isbn_and_issn_map.end()) {
        ++missing_isbn_or_issn_count;
        return SUCCESS;
    }

    if (IsPossibleISSN(parent_isbn_or_issn_iter->second)) {
        subfields.addSubfield('x', parent_isbn_or_issn_iter->second);
        record.updateField(index_773, subfields.toString());
        ++issns_added;
    } else { // Deal with ISBNs.
        if (not record.extractFirstSubfield("020", 'a').empty())
            return SUCCESS; // We already have an ISBN.
        std::string new_field_020("  ""\x1F""a" + parent_isbn_or_issn_iter->second);
        record.insertField("020", new_field_020);
        ++isbns_added;
    }
    return SUCCESS;
};


PhaseAddIsbnsOrIssnsToArticles::~PhaseAddIsbnsOrIssnsToArticles() {
    std::cerr << "Add ISBNs or ISSNs to articles:\n";
    std::cerr << "\tPhaseAddIsbnsOrIssnsToArticles:\n";
    std::cerr << "\tExtracted " << extracted_isbn_count << " ISBNs.\n";
    std::cerr << "\tExtracted " << extracted_issn_count << " ISSNs.\n";
    std::cerr << "\tAdded ISBN's to " << isbns_added << " article record(s).\n";
    std::cerr << "\tAdded ISSN's to " << issns_added << " article record(s).\n";
    std::cerr << "\t" << missing_host_record_ctrl_num_count << " articles had missing host record control number(s).\n";
    std::cerr << "\tFor " << missing_isbn_or_issn_count << " articles no host ISBN nor ISSN was found.\n";
}