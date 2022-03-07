/** \file    marc_grep.cc
 *  \brief   A tool for fancy grepping in MARC-21 datasets.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2020, Library of the University of Tübingen

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
#include "FileUtil.h"
#include "MARC.h"
#include "MarcQueryParser.h"
#include "StringUtil.h"
#include "util.h"


namespace {


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
    "    field_or_subfield_reference              = '\"' , (field_reference | subfield_reference) '\"'\n"
    "    subfield_reference                       = field_reference , subfield_code , { subfield_code }\n"
    "    field_reference                          = tag , [ indicator_specification ]\n"
    "    indicator_specification                  = '[' , indicator , indicator ']'\n"
    "    indicator                                = letter_or_digit | '#'\n"
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
    "  The special indicator '#' is the wildcard indicator and will match any actual indicator value.\n"
    "\n"
    "  Output label format:\n"
    "    label_format = matched_field_or_subfield | control_number | control_number_and_matched_field_or_subfield\n"
    "                   | no_label | marc_binary | marc_xml | control_number_and_traditional\n"
    "\n"
    "  The default output label is the control number followed by a colon followed by the matched field or \n"
    "  subfield followed by a colon.  When the formats are \"marc_binary\" or \"marc_xml\" entire records will always\n"
    "  be copied.\n";


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--input-format=(marc-xml|marc-21)] [--limit count] [--sample-rate rate] "
              << "[--control-number-list list_filename] marc_filename query [output_label_format]\n\n";
    std::cerr << help_text << '\n';
    std::exit(EXIT_FAILURE);
}


void LoadControlNumbers(const std::string &control_numbers_filename, std::unordered_set<std::string> * const control_numbers) {
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(control_numbers_filename));
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        StringUtil::TrimWhite(&line);
        if (not line.empty())
            control_numbers->insert(line);
    }
}


enum OutputLabel {
    MATCHED_FIELD_OR_SUBFIELD_ONLY,
    CONTROL_NUMBER_ONLY,
    CONTROL_NUMBER_AND_MATCHED_FIELD_OR_SUBFIELD,
    TRADITIONAL,
    NO_LABEL,
    MARC_BINARY,
    MARC_XML,
    CONTROL_NUMBER_AND_TRADITIONAL
};


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

    LOG_ERROR("\"" + label_format_candidate + "\" is no valid output label format!");
}


void Emit(const std::string &control_number, const std::string &tag_or_tag_plus_subfield_code, const std::string &contents,
          const OutputLabel output_format) {
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
        std::cout << tag_or_tag_plus_subfield_code.substr(0, 3) << ' ' << StringUtil::Map(contents, '\x1F', '$') << '\n';
        return;
    case NO_LABEL:
        std::cout << contents << '\n';
        return;
    case MARC_BINARY:
    case MARC_XML:
        LOG_ERROR("MARC_BINARY or MARC_XML should never be passed into Emit(0!");
    case CONTROL_NUMBER_AND_TRADITIONAL:
        std::cout << control_number << ':' << tag_or_tag_plus_subfield_code << ':' << StringUtil::Map(contents, '\x1F', '$') << '\n';
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
            contents_ = rhs.contents_;
        }

        return *this;
    }

    const std::string &getTagOrTagPlusSubfieldCode() const { return tag_or_tag_plus_subfield_code_; }
    const std::string &getContents() const { return contents_; }

    bool operator<(const TagAndContents &rhs) const { return tag_or_tag_plus_subfield_code_ > rhs.tag_or_tag_plus_subfield_code_; }
};


void Emit(const std::string &control_number, const OutputLabel output_format,
          std::priority_queue<TagAndContents> * const tags_and_contents) {
    while (not tags_and_contents->empty()) {
        const TagAndContents &tag_and_contents(tags_and_contents->top());
        Emit(control_number, tag_and_contents.getTagOrTagPlusSubfieldCode(), tag_and_contents.getContents(), output_format);
        tags_and_contents->pop();
    }
}


bool EnqueueSubfields(const std::string &tag, const char subfield_code, const std::string &contents,
                      std::priority_queue<TagAndContents> * const tags_and_contents) {
    std::string tag_plus_subfield_code(tag);
    tag_plus_subfield_code += subfield_code;
    const MARC::Subfields subfields(contents);
    bool enqueue_at_least_one(false);
    for (const auto &subfield : subfields) {
        if (subfield.code_ == subfield_code) {
            tags_and_contents->push(TagAndContents(tag_plus_subfield_code, subfield.value_));
            enqueue_at_least_one = true;
        }
    }

    return enqueue_at_least_one;
}


bool ProcessEqualityComp(const ConditionDescriptor &cond_desc, const MARC::Record &record) {
    const std::string comp_field_or_subfield(cond_desc.getFieldOrSubfieldReference());
    const std::string subfield_codes(comp_field_or_subfield.substr(MARC::Record::TAG_LENGTH));
    const ConditionDescriptor::CompType comp_type(cond_desc.getCompType());
    std::string err_msg;

    for (const auto &field : record.getTagRange(comp_field_or_subfield.substr(0, MARC::Record::TAG_LENGTH))) {
        if (subfield_codes.empty()) { // Compare against the entire field. (Does this even make sense?)
            if (cond_desc.getDataMatcher().matched(field.getContents(), &err_msg))
                return comp_type == ConditionDescriptor::EQUAL_EQUAL;
            if (unlikely(not err_msg.empty()))
                LOG_ERROR("match failed (" + err_msg + ")! (1)");
        } else { // We need to match against a subfield's content.
            for (const auto &subfield : field.getSubfields()) {
                if (subfield.code_ == subfield_codes[0]) {
                    if (cond_desc.getDataMatcher().matched(subfield.value_, &err_msg))
                        return comp_type == ConditionDescriptor::EQUAL_EQUAL;
                    else if (unlikely(not err_msg.empty()))
                        LOG_ERROR("match failed (" + err_msg + ")! (1)");
                }
            }
        }
    }

    return comp_type != ConditionDescriptor::EQUAL_EQUAL;
}


bool ProcessExistenceTest(const ConditionDescriptor &cond_desc, const MARC::Record &record) {
    const std::string test_field_or_subfield(cond_desc.getFieldOrSubfieldReference());
    const ConditionDescriptor::CompType comp_type(cond_desc.getCompType());
    const auto tag(test_field_or_subfield.substr(0, MARC::Record::TAG_LENGTH));
    if (not record.hasTag(tag))
        return comp_type == ConditionDescriptor::IS_MISSING;
    const std::string subfield_codes(test_field_or_subfield.substr(MARC::Record::TAG_LENGTH));
    if (subfield_codes.empty())
        return comp_type == ConditionDescriptor::EXISTS;

    for (const auto &field : record.getTagRange(tag)) {
        if (field.hasSubfield(subfield_codes[0]))
            return comp_type == ConditionDescriptor::EXISTS;
    }

    return comp_type != ConditionDescriptor::EXISTS;
}


bool ProcessConditions(const OutputLabel output_format, const ConditionDescriptor &cond_desc,
                       const FieldOrSubfieldDescriptor &field_or_subfield_desc, const MARC::Record &record,
                       std::priority_queue<TagAndContents> * const tags_and_contents) {
    const std::string extraction_tag(field_or_subfield_desc.getTag());
    if (extraction_tag != "*" and not record.hasTag(extraction_tag))
        return false;

    const ConditionDescriptor::CompType comp_type(cond_desc.getCompType());
    if (comp_type == ConditionDescriptor::NO_COMPARISION
        or ((comp_type == ConditionDescriptor::EQUAL_EQUAL or comp_type == ConditionDescriptor::NOT_EQUAL)
            and ProcessEqualityComp(cond_desc, record))
        or ((comp_type == ConditionDescriptor::EXISTS or comp_type == ConditionDescriptor::IS_MISSING)
            and ProcessExistenceTest(cond_desc, record)))
    {
        if (field_or_subfield_desc.isStar()) {
            if (output_format != MARC_BINARY and output_format != MARC_XML) {
                for (const auto &field : record)
                    tags_and_contents->push(TagAndContents(field.getTag().toString(), field.getContents()));
            }
            return true;
        }

        const std::string subfield_codes(field_or_subfield_desc.getSubfieldCodes());
        bool emitted_at_least_one(false);
        for (const auto &field : record.getTagRange(extraction_tag)) {
            if (subfield_codes.empty()) {
                tags_and_contents->push(TagAndContents(extraction_tag, field.getContents()));
                emitted_at_least_one = true;
            } else { // Looking for one or more subfields:
                for (const auto &subfield_code : subfield_codes) {
                    if (EnqueueSubfields(extraction_tag, subfield_code, field.getContents(), tags_and_contents))
                        emitted_at_least_one = true;
                }
            }
        }

        return emitted_at_least_one;
    } else if (comp_type == ConditionDescriptor::SINGLE_FIELD_EQUAL or comp_type == ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL) {
        if (field_or_subfield_desc.isStar()) {
            if (output_format != MARC_BINARY and output_format != MARC_XML) {
                for (const auto &field : record)
                    tags_and_contents->push(TagAndContents(field.getTag().toString(), field.getContents()));
            }
            return true;
        }

        bool emitted_at_least_one(false);
        const char test_subfield_code(cond_desc.getFieldOrSubfieldReference()[MARC::Record::TAG_LENGTH]);
        const char extract_subfield_code(field_or_subfield_desc.getSubfieldCodes()[0]);
        for (const auto &field : record.getTagRange(extraction_tag)) {
            const MARC::Subfields subfields(field.getSubfields());
            if (not subfields.hasSubfield(extract_subfield_code))
                continue;

            if (not subfields.hasSubfield(test_subfield_code)) {
                if (comp_type == ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL) {
                    if (EnqueueSubfields(extraction_tag, extract_subfield_code, field.getContents(), tags_and_contents))
                        emitted_at_least_one = true;
                } else
                    return false;
            } else {
                bool matched_at_least_one(false);
                for (const auto &subfield : subfields) {
                    if (subfield.code_ == test_subfield_code) {
                        std::string err_msg;
                        if (cond_desc.getDataMatcher().matched(subfield.value_, &err_msg)) {
                            matched_at_least_one = true;
                            break;
                        } else if (unlikely(not err_msg.empty()))
                            LOG_ERROR("Unexpected: match failed!");
                    }
                }

                if ((matched_at_least_one and comp_type == ConditionDescriptor::SINGLE_FIELD_EQUAL)
                    or (not matched_at_least_one and comp_type == ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL))
                {
                    if (EnqueueSubfields(extraction_tag, extract_subfield_code, field.getContents(), tags_and_contents))
                        emitted_at_least_one = true;
                }
            }
        }

        return emitted_at_least_one;
    } else
        return false;
}


void FieldGrep(const unsigned max_records, const unsigned sampling_rate, const std::unordered_set<std::string> &control_numbers,
               MARC::Reader * const marc_reader, const QueryDescriptor &query_desc, const OutputLabel output_format) {
    std::unique_ptr<MARC::Writer> marc_writer(nullptr);
    if (output_format == MARC_BINARY or output_format == MARC_XML)
        marc_writer = MARC::Writer::Factory("/proc/self/fd/1", (output_format == MARC_XML) ? MARC::FileType::XML : MARC::FileType::BINARY);

    std::string err_msg;
    unsigned count(0), matched_count(0), rate_counter(0);
    while (const MARC::Record record = marc_reader->read()) {
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

        if (query_desc.hasLeaderCondition()) {
            const LeaderCondition &leader_cond(query_desc.getLeaderCondition());
            const std::string &leader(record.getLeader());
            if (leader.substr(leader_cond.getStartOffset(), leader_cond.getEndOffset() - leader_cond.getStartOffset() + 1)
                != leader_cond.getMatch())
                continue;
        }

        bool matched(false);
        std::priority_queue<TagAndContents> tags_and_contents;

        // Extract fields and subfields:
        for (const auto &cond_and_field_or_subfield : query_desc.getCondsAndFieldOrSubfieldDescs()) {
            if (ProcessConditions(output_format, cond_and_field_or_subfield.first, cond_and_field_or_subfield.second, record,
                                  &tags_and_contents)) {
                matched = true;
                if (output_format == MARC_BINARY or output_format == MARC_XML)
                    break;
            }
        }

        if (matched) {
            ++matched_count;

            if (output_format == MARC_BINARY or output_format == MARC_XML)
                marc_writer->write(record);
            else {
                // Determine the control number:
                const auto control_number(record.getControlNumber());
                if (unlikely(control_number.empty()))
                    LOG_ERROR("record has no control number!");

                Emit(control_number, output_format, &tags_and_contents);
            }
        }
    }

    if (not err_msg.empty())
        logger->error(err_msg);
    std::cerr << "Matched " << matched_count << (matched_count == 1 ? " record of " : " records of ") << count << " overall records.\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    MARC::FileType reader_type(MARC::FileType::AUTO);
    if (argc > 1 and std::strncmp(argv[1], "--input-format=", __builtin_strlen("--input-format=")) == 0) {
        if (std::strcmp(argv[1] + __builtin_strlen("--input-format="), "marc-xml") == 0)
            reader_type = MARC::FileType::XML;
        else if (std::strcmp(argv[1] + __builtin_strlen("--input-format="), "marc-21") == 0)
            reader_type = MARC::FileType::BINARY;
        else
            LOG_ERROR("input format must be \"marc-xml\" or \"marc-21\"!");
        --argc, ++argv;
    }

    // Limit the number of records that we will process:
    unsigned max_records(UINT_MAX);
    if (argc > 1 and std::strcmp(argv[1], "--limit") == 0) {
        if (argc <= 3)
            Usage();

        if (not StringUtil::ToUnsigned(argv[2], &max_records))
            logger->error("bad record count limit: \"" + std::string(argv[2]) + "\"!");
        argc -= 2;
        argv += 2;
    }

    unsigned sampling_rate(1);
    if (argc > 1 and std::strcmp(argv[1], "--sample-rate") == 0) {
        if (argc <= 3)
            Usage();

        if (not StringUtil::ToUnsigned(argv[2], &sampling_rate))
            logger->error("bad sampling rate: \"" + std::string(argv[2]) + "\"!");
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

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1], reader_type));

    std::unordered_set<std::string> control_numbers;
    if (not control_numbers_filename.empty())
        LoadControlNumbers(control_numbers_filename, &control_numbers);

    QueryDescriptor query_desc;
    std::string err_msg;
    if (not ParseQuery(argv[2], &query_desc, &err_msg))
        logger->error("Query parsing failed: " + err_msg);

    const OutputLabel output_label = (argc == 4) ? ParseOutputLabel(argv[3]) : CONTROL_NUMBER_AND_MATCHED_FIELD_OR_SUBFIELD;
    FieldGrep(max_records, sampling_rate, control_numbers, marc_reader.get(), query_desc, output_label);

    return EXIT_SUCCESS;
}
