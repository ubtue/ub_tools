/** \file    marc_grep2.cc
 *  \brief   A tool for fancy grepping in MARC-21 datasets.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015, 2016, Library of the University of Tübingen

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
#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <climits>
#include <cstring>
#include <unistd.h>
#include "Compiler.h"
#include "DirectoryEntry.h"
#include "FileUtil.h"
#include "Leader.h"
#include "MarcQueryParser.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "Subfields.h"
#include "util.h"
#include "MarcXmlWriter.h"


char help_text[] =
  "  \"--limit\"  Only process the first \"count\" records.\n"
  "  \"--sample-rate\"  Only process every \"rate\"-th record.\n"
  "  \"--control-number-list\"  Only process records whose control numbers are listed in the specified file.\n"
  "\n"
  "  Query syntax:\n"
  "    query                                    = [ leader_condition ] simple_query\n"
  "    leader_condition                         = \"leader[\" offset_range \"]=\" string_constant\n"
  "    offset_range                             = start_offset [ \"-\" end_offset ]\n"
  "    start_offset                             = unsigned_integer\n"
  "    end_offset                               = unsigned_integer\n"
  "    unsigned_integer                         = digit { digit }\n"
  "    digit                                    = \"0\" | \"1\" | \"2\" | \"3\" | \"4\" | \"5\" | \"6\" | \"7\"\n"
  "                                               | \"8\" | \"9\"\n"
  "    simple_query                             = simple_field_list | conditional_field_or_subfield_references\n"
  "    simple_field_list                        = field_or_subfield_reference\n"
  "                                               { \":\" field_or_subfield_reference }\n"
  "    field_or_subfield_reference              = field_reference | subfield_reference\n"
  "    conditional_field_or_subfield_references = conditional_field_or_subfield_reference\n"
  "                                               { \",\" conditional_field_or_subfield_reference }\n"
  "    conditional_field_or_subfield_reference  = \"if\" condition \"extract\"\n"
  "                                               (field_or_subfield_reference | \"*\")\n"
  "    condition                                = field_or_subfield_reference comp_op reg_ex\n"
  "                                               | field_or_subfield_reference \"exists\"\n"
  "                                               | field_or_subfield_reference \"is_missing\"\n"
  "    reg_ex                                   = string_constant\n"
  "    comp_op                                  = \"==\" | \"!=\" | \"===\" | \"!==\"\n"
  "\n"
  "  String constants start and end with double quotes. Backslashes and double quotes within need to be escaped\n"
  "  with a backslash. The difference between the \"==\" and \"!=\" vs. \"===\" and \"!===\" comparision\n"
  "  operators is that the latter compares subfields within a given field while the former compares against any two\n"
  "  matching fields or subfields.  This becomes relevant when there are multiple occurrences of a field in a\n"
  "  record. \"*\" matches all fields.  Field and subfield references are strings and thus need to be quoted.\n"
  "\n"
  "  Output label format:\n"
  "    label_format = matched_field_or_subfield | control_number | control_number_and_matched_field_or_subfield\n"
  "                   | no_label | marc_binary | marc_xml | control_number_and_traditional\n"
  "\n"
  "  The default output label is the control number followed by a colon followed by the matched field or \n"
  "  subfield followed by a colon.  When the formats are \"marc_binary\" or \"marc_xml\" entire records will always\n"
  "  be copied.\n";


void Usage() {
    std::cerr << "Usage: " << progname << " [--limit count] [--sample-rate rate] "
              << "[--control-number-list list_filename] marc_filename query [output_label_format]\n\n";
    std::cerr << help_text << '\n';
    std::exit(EXIT_FAILURE);
}


void LoadControlNumbers(const std::string &control_numbers_filename,
                        std::unordered_set<std::string> * const control_numbers)
{
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(control_numbers_filename));
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        StringUtil::TrimWhite(&line);
        if (not line.empty())
            control_numbers->insert(line);
    }
}


enum OutputLabel { MATCHED_FIELD_OR_SUBFIELD_ONLY, CONTROL_NUMBER_ONLY, CONTROL_NUMBER_AND_MATCHED_FIELD_OR_SUBFIELD,
                   TRADITIONAL, NO_LABEL, MARC_BINARY, MARC_XML, CONTROL_NUMBER_AND_TRADITIONAL };


OutputLabel ParseOutputLabel(const std::string &label_format_candidate) {
    if (label_format_candidate == "matched_field_or_subfield")
        return MATCHED_FIELD_OR_SUBFIELD_ONLY;
    if (label_format_candidate == "control_number")
        return CONTROL_NUMBER_ONLY;
    if (label_format_candidate == "control_number_and_matched_field_or_subfield")
        return CONTROL_NUMBER_AND_MATCHED_FIELD_OR_SUBFIELD;
    if (label_format_candidate == "traditional")
        return TRADITIONAL;
    if (label_format_candidate == "no_label")
        return NO_LABEL;
    if (label_format_candidate == "marc_binary")
        return MARC_BINARY;
    if (label_format_candidate == "marc_xml")
        return MARC_XML;
    if (label_format_candidate == "control_number_and_traditional")
        return CONTROL_NUMBER_AND_TRADITIONAL;

    Error("\"" + label_format_candidate + "\" is no valid output label format!");
}


void Emit(const std::string &control_number, const std::string &tag_or_tag_plus_subfield_code,
          const std::string &contents, const OutputLabel output_format)
{
    switch (output_format) {
    case MATCHED_FIELD_OR_SUBFIELD_ONLY:
        std::cout << tag_or_tag_plus_subfield_code << ':' << contents << '\n';
        return;
    case CONTROL_NUMBER_ONLY:
        std::cout << control_number << ':' << contents << '\n';
        return;
    case CONTROL_NUMBER_AND_MATCHED_FIELD_OR_SUBFIELD:
        std::cout << control_number << ':' << tag_or_tag_plus_subfield_code << ':' << contents << '\n';
        return;
    case TRADITIONAL:
        std::cout << tag_or_tag_plus_subfield_code.substr(0, 3) << ' ' << StringUtil::Map(contents, '\x1F', '$')
                  << '\n';
        return;
    case NO_LABEL:
        std::cout << contents << '\n';
        return;
    case MARC_BINARY:
    case MARC_XML:
        Error("MARC_BINARY or MARC_XML should never be passed into Emit(0!");
    case CONTROL_NUMBER_AND_TRADITIONAL:
        std::cout << control_number << ':' << tag_or_tag_plus_subfield_code << ':'
                  << StringUtil::Map(contents, '\x1F', '$') << '\n';
        return;
    }
}


class TagAndContents {
    std::string tag_or_tag_plus_subfield_code_;
    std::string contents_;
public:
    TagAndContents(const std::string &tag_or_tag_plus_subfield_code, const std::string &contents)
        : tag_or_tag_plus_subfield_code_(tag_or_tag_plus_subfield_code), contents_(contents) { }

    TagAndContents(TagAndContents &&other) {
        std::swap(tag_or_tag_plus_subfield_code_, other.tag_or_tag_plus_subfield_code_);
        std::swap(contents_, other.contents_);
    }

    TagAndContents &operator=(const TagAndContents &rhs) {
        if (&rhs != this) {
            tag_or_tag_plus_subfield_code_ = rhs.tag_or_tag_plus_subfield_code_;
            contents_                      = rhs.contents_;
        }

        return *this;
    }

    const std::string &getTagOrTagPlusSubfieldCode() const { return tag_or_tag_plus_subfield_code_; }
    const std::string &getContents() const { return contents_; }

    bool operator<(const TagAndContents &rhs) const 
        { return tag_or_tag_plus_subfield_code_ > rhs.tag_or_tag_plus_subfield_code_; }
};


void Emit(const std::string &control_number, const OutputLabel output_format,
          std::priority_queue<TagAndContents> * const tags_and_contents)
{
    while (not tags_and_contents->empty()) {
        const TagAndContents &tag_and_contents(tags_and_contents->top());
        Emit(control_number, tag_and_contents.getTagOrTagPlusSubfieldCode(), tag_and_contents.getContents(),
             output_format);
        tags_and_contents->pop();
    }
}


bool EnqueueSubfields(const std::string &tag, const char subfield_code, const std::string &contents,
                      std::priority_queue<TagAndContents> * const tags_and_contents)
{
    std::string tag_plus_subfield_code(tag);
    tag_plus_subfield_code += subfield_code;
    const Subfields subfields(contents);
    const auto begin_end(subfields.getIterators(subfield_code));
    bool emitted_at_least_one(false);
    for (auto code_and_subfield(begin_end.first); code_and_subfield != begin_end.second; ++code_and_subfield) {
        tags_and_contents->push(TagAndContents(tag_plus_subfield_code, code_and_subfield->second));
        emitted_at_least_one = true;
    }

    return emitted_at_least_one;
}


bool ProcessEqualityComp(const ConditionDescriptor &cond_desc,
                         const std::unordered_multimap<std::string, const std::string *> &field_to_content_map)
{
    const FieldOrSubfieldDescriptor comp_field_or_subfield(cond_desc.getFieldOrSubfieldReference());
    const auto begin_end(field_to_content_map.equal_range(comp_field_or_subfield.getTag()));
    const std::string subfield_codes(comp_field_or_subfield.getSubfieldCodes());
    const ConditionDescriptor::CompType comp_type(cond_desc.getCompType());
    std::string err_msg;
    bool matched_at_least_one(false);
    for (auto field(begin_end.first); field != begin_end.second; ++field) {
        const std::string &contents(*(field->second));
        if (subfield_codes.empty()) { // Compare against the entire field. (Does this even make sense?)
            if (cond_desc.getDataMatcher().matched(contents, &err_msg)) {
                matched_at_least_one = true;
                break;
            }
            if (unlikely(not err_msg.empty()))
                Error("ProcessEqualityComp: match failed (" + err_msg + ")! (1)");
        } else  { // We need to match against a subfield's content.
        const Subfields subfields(contents);
        if (not subfields.hasSubfield(subfield_codes[0]))
            continue;
        const auto sub_begin_end(subfields.getIterators(subfield_codes[0]));
        for (auto subfield_contents(sub_begin_end.first); subfield_contents != sub_begin_end.second;
             ++subfield_contents)
            {
                if (cond_desc.getDataMatcher().matched(subfield_contents->second, &err_msg)) {
                    matched_at_least_one = true;
                    break;
                } else if (unlikely(not err_msg.empty()))
                    Error("ProcessEqualityComp: match failed (" + err_msg + ")! (1)");
            }
        }
    }

    return (comp_type == ConditionDescriptor::EQUAL_EQUAL) ? matched_at_least_one : not matched_at_least_one;
}


bool ProcessExistenceTest(const ConditionDescriptor &cond_desc,
                          const std::unordered_multimap<std::string, const std::string *> &field_to_content_map)
{
    const FieldOrSubfieldDescriptor test_field_or_subfield(cond_desc.getFieldOrSubfieldReference());
    const ConditionDescriptor::CompType comp_type(cond_desc.getCompType());
    const auto begin_end(field_to_content_map.equal_range(test_field_or_subfield.getTag()));
    if (begin_end.first == begin_end.second)
        return comp_type == ConditionDescriptor::IS_MISSING;
    const std::string subfield_codes(test_field_or_subfield.getSubfieldCodes());
    if (subfield_codes.empty())
        return comp_type == ConditionDescriptor::EXISTS;

    bool found_at_least_one(false);
    for (auto field(begin_end.first); field != begin_end.second; ++field) {
        const std::string &contents(*(field->second));
        const Subfields subfields(contents);
        if (subfields.hasSubfield(subfield_codes[0])) {
            found_at_least_one = true;
            break;
        }
    }

    return (comp_type == ConditionDescriptor::EXISTS) ? found_at_least_one : not found_at_least_one;
}


bool ProcessConditions(const ConditionDescriptor &cond_desc, const FieldOrSubfieldDescriptor &field_or_subfield_desc,
                       const std::unordered_multimap<std::string, const std::string *> &field_to_content_map,
                       std::priority_queue<TagAndContents> * const tags_and_contents)
{
    const std::string extraction_tag(field_or_subfield_desc.getTag());
    const auto begin_end(field_to_content_map.equal_range(extraction_tag));
    const bool extraction_tag_found(begin_end.first != begin_end.second or field_or_subfield_desc.isStar());
    if (not extraction_tag_found)
        return false;

    const ConditionDescriptor::CompType comp_type(cond_desc.getCompType());
    if (comp_type == ConditionDescriptor::NO_COMPARISION
        or ((comp_type == ConditionDescriptor::EQUAL_EQUAL or comp_type == ConditionDescriptor::NOT_EQUAL)
            and ProcessEqualityComp(cond_desc, field_to_content_map))
        or ((comp_type == ConditionDescriptor::EXISTS or comp_type == ConditionDescriptor::IS_MISSING)
            and ProcessExistenceTest(cond_desc, field_to_content_map)))
    {
        if (field_or_subfield_desc.isStar()) {
            for (const auto &tag_and_content : field_to_content_map)
                tags_and_contents->push(TagAndContents(tag_and_content.first, *(tag_and_content.second)));
            return true;
        }

        const std::string subfield_codes(field_or_subfield_desc.getSubfieldCodes());
        bool emitted_at_least_one(false);
        for (auto tag_and_field_contents(begin_end.first); tag_and_field_contents != begin_end.second;
             ++tag_and_field_contents)
        {
            if (subfield_codes.empty()) {
                tags_and_contents->push(TagAndContents(extraction_tag, *(tag_and_field_contents->second)));
                emitted_at_least_one = true;
            } else { // Looking for one or more subfields:
                for (const auto &subfield_code : subfield_codes) {
                    if (EnqueueSubfields(extraction_tag, subfield_code, *tag_and_field_contents->second,
                                         tags_and_contents))
                        emitted_at_least_one = true;
                }
            }
        }

        return emitted_at_least_one;
    } else if (comp_type == ConditionDescriptor::SINGLE_FIELD_EQUAL
               or comp_type == ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL)
    {
        if (field_or_subfield_desc.isStar()) {
            for (const auto &tag_and_content : field_to_content_map)
                tags_and_contents->push(TagAndContents(tag_and_content.first, *(tag_and_content.second)));
            return true;
        }

        bool emitted_at_least_one(false);
        const char test_subfield_code(cond_desc.getFieldOrSubfieldReference()[DirectoryEntry::TAG_LENGTH]);
        const char extract_subfield_code(field_or_subfield_desc.getSubfieldCodes()[0]);
        for (auto tag_and_field_contents(begin_end.first); tag_and_field_contents != begin_end.second;
             ++tag_and_field_contents)
        {
            const Subfields subfields(*tag_and_field_contents->second);
            if (not subfields.hasSubfield(extract_subfield_code))
                continue;

            if (not subfields.hasSubfield(test_subfield_code)) {
                if (comp_type == ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL) {
                    if (EnqueueSubfields(extraction_tag, extract_subfield_code, *tag_and_field_contents->second,
                                         tags_and_contents))
                        emitted_at_least_one = true;
                } else
                    return false;
            } else {
                bool matched_at_least_one(false);
                const auto sub_begin_end(subfields.getIterators(test_subfield_code));
                std::string err_msg;
                for (auto code_and_value(sub_begin_end.first); code_and_value != sub_begin_end.second;
                     ++code_and_value)
                {
                    if (cond_desc.getDataMatcher().matched(code_and_value->second, &err_msg)) {
                        matched_at_least_one = true;
                        break;
                    } else if (unlikely(not err_msg.empty()))
                        Error("Unexpected: Match failed in ProcessConditions!");
                }

                if ((matched_at_least_one and comp_type == ConditionDescriptor::SINGLE_FIELD_EQUAL)
                    or (not matched_at_least_one and comp_type == ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL))
                {
                    if (EnqueueSubfields(extraction_tag, extract_subfield_code, *tag_and_field_contents->second,
                                         tags_and_contents))
                        emitted_at_least_one = true;
                }
            }
        }

        return emitted_at_least_one;
    } else
        return false;
}


void FieldGrep(const unsigned max_records, const unsigned sampling_rate,
               const std::unordered_set<std::string> &control_numbers, const std::string &input_filename,
               const QueryDescriptor &query_desc, const OutputLabel output_format)
{
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(input_filename));

    const std::string media_type(MediaTypeUtil::GetFileMediaType(input_filename));
    if (unlikely(media_type.empty()))
        Error("can't determine media type of \"" + input_filename + "\"!");
    if (media_type != "application/xml" and media_type != "application/marc")
        Error("\"" + input_filename + "\" is neither XML nor MARC-21 data!");
    const bool input_is_xml(media_type == "application/xml");

    File output(STDOUT_FILENO);
    std::string err_msg;
    unsigned count(0), matched_count(0), rate_counter(0);

    std::unique_ptr<MarcXmlWriter> xml_writer;
    if (output_format == MARC_XML)
        xml_writer.reset(new MarcXmlWriter(&output));

    while (MarcUtil::Record record = input_is_xml ? MarcUtil::Record::XmlFactory(input.get())
                                                  : MarcUtil::Record::BinaryFactory(input.get()))
    {
        // If we use a control number filter, only process a record if it is in our list:
        if (not control_numbers.empty() and control_numbers.find(record.getControlNumber()) == control_numbers.cend())
            continue;

        ++count, ++rate_counter;
        if (count > max_records)
            break;
        if (rate_counter == sampling_rate)
            rate_counter = 0;
        else
            continue;

        if (output_format == MARC_XML)
            record.setRecordWillBeWrittenAsXml(true);

        if (query_desc.hasLeaderCondition()) {
            const LeaderCondition &leader_cond(query_desc.getLeaderCondition());
            const Leader &leader(record.getLeader());
            if (leader.toString().substr(leader_cond.getStartOffset(),
                                         leader_cond.getEndOffset() - leader_cond.getStartOffset() + 1)
                != leader_cond.getMatch())
                continue;
        }

        const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        const std::vector<std::string> &fields(record.getFields());
        std::unordered_multimap<std::string, const std::string *> field_to_content_map;
        for (unsigned i(0); i < dir_entries.size(); ++i)
            field_to_content_map.insert(std::make_pair(dir_entries[i].getTag(), &fields[i]));

        bool matched(false);
        std::priority_queue<TagAndContents> tags_and_contents;

        // Extract fields and subfields:
        for (const auto &cond_and_field_or_subfield : query_desc.getCondsAndFieldOrSubfieldDescs()) {
            if (ProcessConditions(cond_and_field_or_subfield.first, cond_and_field_or_subfield.second,
                                  field_to_content_map, &tags_and_contents))
                matched = true;
        }

        if (matched) {
            ++matched_count;

            if (output_format == MARC_BINARY)
                record.write(&output);
            else if (output_format == MARC_XML)
                record.write(xml_writer.get());
            else {
                // Determine the control number:
                const auto &control_number_iter(field_to_content_map.find("001"));
                if (unlikely(control_number_iter == field_to_content_map.end()))
                    Error("In FieldGrep: record has no control number!");
                const std::string control_number(*(control_number_iter->second));

                Emit(control_number, output_format, &tags_and_contents);
            }
        }
    }

    if (not err_msg.empty())
        Error(err_msg);
    std::cerr << "Matched " << matched_count << (matched_count == 1 ? " record of " :  " records of ") << count
              << " overall records.\n";
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    // Limit the number of records that we will process:
    unsigned max_records(UINT_MAX);
    if (argc > 1 and std::strcmp(argv[1], "--limit") == 0) {
        if (argc <= 3)
            Usage();

        if (not StringUtil::ToUnsigned(argv[2], &max_records))
            Error("bad record count limit: \"" + std::string(argv[2]) + "\"!");
        argc -= 2;
        argv += 2;
    }

    unsigned sampling_rate(1);
    if (argc > 1 and std::strcmp(argv[1], "--sample-rate") == 0) {
        if (argc <= 3)
            Usage();

        if (not StringUtil::ToUnsigned(argv[2], &sampling_rate))
            Error("bad sampling rate: \"" + std::string(argv[2]) + "\"!");
        argc -= 2;
        argv += 2;
    }

    if (argc < 3)
        Usage();

    std::string control_numbers_filename;
    if (std::strcmp("--control-number-list", argv[1]) == 0) {
        control_numbers_filename = argv[2];
        argc -= 2;
        argv += 2;
    }

    if (argc < 3 or argc > 4)
        Usage();

    try {
        std::unordered_set<std::string> control_numbers;
        if (not control_numbers_filename.empty())
            LoadControlNumbers(control_numbers_filename, &control_numbers);

        QueryDescriptor query_desc;
        std::string err_msg;
        if (not ParseQuery(argv[2], &query_desc, &err_msg))
            Error("Query parsing failed: " + err_msg);

        const OutputLabel output_label = (argc == 4) ? ParseOutputLabel(argv[3])
            : CONTROL_NUMBER_AND_MATCHED_FIELD_OR_SUBFIELD;
        FieldGrep(max_records, sampling_rate, control_numbers, argv[1], query_desc, output_label);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
