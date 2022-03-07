/** \brief Utility for searching for MARC records matching multiple conditions.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "marc_data [output=tag_or_tag_plus_subfield_code] tag_and_subfield_code1=pattern1 [tag_and_subfield_code2=pattern2 ... "
        "tag_and_subfield_code3=pattern3]\n"
        "where pattern1 through patternN are PCREs.\n"
        "If no output has been specified, then only the control numbers of the matching records will be displayed.\n");
}


class Query {
    MARC::Tag tag_;
    char subfield_code_;
    RegexMatcher *matcher_;

public:
    Query() = default;
    Query(const Query &other) = default;
    Query(const std::string &tag, const char subfield_code, RegexMatcher * const matcher)
        : tag_(tag), subfield_code_(subfield_code), matcher_(matcher) { }
    bool operator<(const Query &rhs) const;
    inline const MARC::Tag &getTag() const { return tag_; }
    inline char getSubfieldCode() const { return subfield_code_; }
    bool matched(const std::string &subfield_contents) const { return matcher_->matched(subfield_contents); }
};


bool Query::operator<(const Query &rhs) const {
    if (tag_ < rhs.tag_)
        return true;
    if (tag_ > rhs.tag_)
        return false;
    return subfield_code_ < rhs.subfield_code_;
}


// Create a human-readable representation of the contents of "field".
std::string FieldContentsToString(const MARC::Record::Field &field) {
    if (field.isControlField())
        return field.getContents();

    std::string as_string;
    for (const auto &subfield : field.getSubfields())
        as_string += "$" + std::string(1, subfield.code_) + subfield.value_;
    return as_string;
}


void GenerateReport(const std::string &output, const MARC::Record &record) {
    if (output.empty())
        std::cout << record.getControlNumber() << '\n';
    else {
        static const MARC::Tag output_tag(output.substr(0, MARC::Record::TAG_LENGTH));
        static const char output_subfield_code(output.length() == MARC::Record::TAG_LENGTH ? '\0' : output[MARC::Record::TAG_LENGTH]);
        for (const auto &output_field : record.getTagRange(output_tag)) {
            if (output_subfield_code == '\0') {
                std::cout << record.getControlNumber() << ": " << FieldContentsToString(output_field) << '\n';
            } else {
                for (const auto &subfield_code_and_value : output_field.getSubfields()) {
                    if (subfield_code_and_value.code_ == output_subfield_code)
                        std::cout << record.getControlNumber() << ": " << subfield_code_and_value.value_ << '\n';
                }
            }
        }
    }
}


void ProcessRecords(const std::vector<Query> &queries, const std::string &output, MARC::Reader * const marc_reader) {
    unsigned record_count(0), matched_count(0);

next_record:
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        for (const auto &query : queries) {
            for (const auto &field : record.getTagRange(query.getTag())) {
                for (const auto &subfield : field.getSubfields()) {
                    if (subfield.code_ == query.getSubfieldCode() and query.matched(subfield.value_))
                        goto next_query;
                }
            }
            goto next_record; // We found no match for the current query!
next_query:
    /* Intentionally empty! */;
        }

        GenerateReport(output, record);
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " record(s) of which " + std::to_string(matched_count) + " record(s) matched.");
}


Query ProcessQuery(const std::string &query_string) {
    const auto equal_pos(query_string.find('='));
    if (equal_pos == std::string::npos or equal_pos != MARC::Record::TAG_LENGTH + 1)
        LOG_ERROR("bad query \"" + query_string + "\"!");

    std::string error_message;
    const auto matcher(RegexMatcher::RegexMatcherFactory(query_string.substr(equal_pos + 1), &error_message));
    if (matcher == nullptr)
        LOG_ERROR("bad query \"" + query_string + "\"! (" + error_message + ")");

    return Query(query_string.substr(0, MARC::Record::TAG_LENGTH), query_string[MARC::Record::TAG_LENGTH], matcher);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    std::string output;
    int query_start;
    if (StringUtil::StartsWith(argv[2], "output=")) {
        if (argc < 4)
            Usage();
        output = argv[2] + __builtin_strlen("output=");
        query_start = 3;
    } else
        query_start = 2;

    std::vector<Query> queries;
    for (int arg_no(query_start); arg_no < argc; ++arg_no)
        queries.emplace_back(ProcessQuery(argv[arg_no]));
    std::sort(queries.begin(), queries.end());

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    ProcessRecords(queries, output, marc_reader.get());

    return EXIT_SUCCESS;
}
