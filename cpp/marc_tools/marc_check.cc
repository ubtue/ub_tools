/** \brief Utility for displaying various bits of info about a collection of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <map>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "[--do-not-abort-on-empty-subfields] [--do-not-abort-on-invalid-repeated-fields] [--check-rule-violations-only]"
        " [--write-data=output_filename] marc_data [rules violated_rules_control_number_list]\n"
        "       If \"--write-data\" has been specified, the read records will be written out again.\n");
    std::exit(EXIT_FAILURE);
}


class Rule {
public:
    virtual ~Rule() { }

    virtual bool hasBeenViolated(const MARC::Record &record, std::string * const err_msg) const = 0;
};


class SubfieldMatches final : public Rule {
    MARC::Tag tag_;
    char indicator1_, indicator2_;
    char subfield_code_;
    std::shared_ptr<RegexMatcher> matcher_;

public:
    SubfieldMatches(const MARC::Tag &tag, const char indicator1, const char indicator2, const char subfield_code,
                    RegexMatcher * const matcher)
        : tag_(tag), indicator1_(indicator1), indicator2_(indicator2), subfield_code_(subfield_code), matcher_(matcher) { }
    virtual ~SubfieldMatches() = default;

    virtual bool hasBeenViolated(const MARC::Record &record, std::string * const err_msg) const final;
};


bool SubfieldMatches::hasBeenViolated(const MARC::Record &record, std::string * const err_msg) const {
    for (const auto &field : record.getTagRange(tag_)) {
        if (indicator1_ == '#' or field.getIndicator1() != indicator1_)
            continue;
        if (indicator2_ == '#' or field.getIndicator2() != indicator2_)
            continue;

        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == subfield_code_ and not matcher_->matched(subfield.value_)) {
                *err_msg = "\"" + subfield.value_ + "\" does not match \"" + matcher_->getPattern() + "\"";
                return true;
            }
        }
    }

    return false;
}


class FirstSubfieldMatches final : public Rule {
    MARC::Tag tag_;
    char indicator1_, indicator2_;
    char subfield_code_;
    std::shared_ptr<RegexMatcher> matcher_;

public:
    FirstSubfieldMatches(const MARC::Tag &tag, const char indicator1, const char indicator2, const char subfield_code,
                         RegexMatcher * const matcher)
        : tag_(tag), indicator1_(indicator1), indicator2_(indicator2), subfield_code_(subfield_code), matcher_(matcher) { }
    virtual ~FirstSubfieldMatches() = default;

    virtual bool hasBeenViolated(const MARC::Record &record, std::string * const err_msg) const final;
};


bool FirstSubfieldMatches::hasBeenViolated(const MARC::Record &record, std::string * const err_msg) const {
    for (const auto &field : record.getTagRange(tag_)) {
        if (indicator1_ == '#' or field.getIndicator1() != indicator1_)
            continue;
        if (indicator2_ == '#' or field.getIndicator2() != indicator2_)
            continue;

        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == subfield_code_ and not matcher_->matched(subfield.value_)) {
                *err_msg = "\"" + subfield.value_ + "\" does not match \"" + matcher_->getPattern() + "\"";
                return true;
            } else if (subfield.code_ == subfield_code_ and matcher_->matched(subfield.value_))
                break;
        }
    }

    return false;
}


// Parse a line with "words" separated by spaces.  Backslash escapes are supported.
bool ParseLine(const std::string &line, std::vector<std::string> * const parts) {
    parts->clear();

    std::string current_part;
    bool escaped(false);
    for (const char ch : line) {
        if (escaped) {
            current_part += ch;
            escaped = false;
        } else if (ch == '\\')
            escaped = true;
        else if (ch == ' ') {
            parts->emplace_back(current_part);
            current_part.clear();
        } else
            current_part += ch;
    }
    if (not current_part.empty())
        parts->emplace_back(current_part);

    return not escaped and not parts->empty();
}


void LoadRules(const std::string &rules_filename, std::vector<Rule *> * const rules) {
    unsigned line_no(0);
    for (const auto line : FileUtil::ReadLines(rules_filename, FileUtil::ReadLines::DO_NOT_TRIM)) {
        ++line_no;

        // Allow hash-comment lines:
        if (not line.empty() and line[0] == '#')
            continue;

        std::vector<std::string> parts;
        if (not ParseLine(line, &parts) or parts.empty())
            LOG_ERROR("bad rule in \"" + rules_filename + "\" on line #" + std::to_string(line_no) + "!");

        if (parts[0] == "subfield_match" or parts[0] == "first_subfield_match") {
            if (parts.size() != 4)
                LOG_ERROR("bad subfield_match rule in \"" + rules_filename + "\" on line #" + std::to_string(line_no) + "!");

            // Indicators
            if (parts[1].length() != 2)
                LOG_ERROR("there need to be two indicators on line #" + std::to_string(line_no) + "!");
            const char indicator1(parts[1][0]), indicator2(parts[1][1]);

            if (parts[2].length() != MARC::Record::TAG_LENGTH + 1)
                LOG_ERROR("bad " + parts[0] + " rule in \"" + rules_filename + "\" on line #" + std::to_string(line_no)
                          + "! (Bad tag and subfield code.)");

            std::string err_msg;
            const auto matcher(RegexMatcher::RegexMatcherFactory(parts[3], &err_msg));
            if (matcher == nullptr)
                LOG_ERROR("bad " + parts[0] + " rule in \"" + rules_filename + "\" on line #" + std::to_string(line_no)
                          + "! (Bad regex: " + err_msg + ".)");

            if (parts[0] == "subfield_match")
                rules->emplace_back(new SubfieldMatches(parts[2].substr(0, MARC::Record::TAG_LENGTH), indicator1, indicator2,
                                                        parts[2][MARC::Record::TAG_LENGTH], matcher));
            else
                rules->emplace_back(new FirstSubfieldMatches(parts[2].substr(0, MARC::Record::TAG_LENGTH), indicator1, indicator2,
                                                             parts[2][MARC::Record::TAG_LENGTH], matcher));
        } else
            LOG_ERROR("unknown rule \"" + parts[0] + "\" in \"" + rules_filename + "\" on line #" + std::to_string(line_no) + "!");
    }
}


void CheckFieldOrder(const bool do_not_abort_on_invalid_repeated_fields, const MARC::Record &record) {
    MARC::Tag last_tag;
    for (const auto &field : record) {
        const MARC::Tag current_tag(field.getTag());
        if (unlikely(current_tag < last_tag))
            LOG_ERROR("invalid tag order in the record with control number \"" + record.getControlNumber() + "\" (\"" + last_tag.toString()
                      + "\" followed by \"" + current_tag.toString() + "\")!");
        if (unlikely(not MARC::IsRepeatableField(current_tag) and current_tag == last_tag)) {
            if (do_not_abort_on_invalid_repeated_fields)
                LOG_WARNING("repeated non-repeatable tag \"" + current_tag.toString() + "\" found in the record with control number \""
                            + record.getControlNumber() + "\"!");
            else
                LOG_ERROR("repeated non-repeatable tag \"" + current_tag.toString() + "\" found in the record with control number \""
                          + record.getControlNumber() + "\"!");
        }
        last_tag = current_tag;
    }
}


void CheckDataField(const bool do_not_abort_on_empty_subfields, const MARC::Record::Field &data_field, const std::string &control_number) {
    const std::string &contents(data_field.getContents());

    if (contents.length() < 5) // Need at least 2 indicators a delimiter a subfield code + subfield contents
        LOG_ERROR("short data field in record w/ control number \"" + control_number + "\"!");

    if (contents[2] != '\x1F')
        LOG_ERROR("first subfield delimiter is missing for the record w/ control number \"" + control_number + "\"!");

    // Check the subfield structure for consistency:
    bool delimiter_seen(false), subfield_code_seen(false);
    for (const char ch : contents) {
        if (delimiter_seen) {
            delimiter_seen = false;
            subfield_code_seen = true;
        } else if (ch == '\x1F') {
            if (unlikely(subfield_code_seen)) {
                if (do_not_abort_on_empty_subfields)
                    LOG_WARNING("empty subfield in a " + data_field.getTag().toString() + "-field in the record w/ control number \""
                                + control_number + "\"!");
                else
                    LOG_ERROR("empty subfield in a " + data_field.getTag().toString() + "-field in the record w/ control number \""
                              + control_number + "\"!");
            }
            delimiter_seen = true;
        } else
            subfield_code_seen = false;
    }

    if (unlikely(delimiter_seen))
        LOG_ERROR("subfield delimiter at end of " + data_field.getTag().toString() + "-field in record w/ control number \""
                  + control_number + "\"!");
    if (unlikely(subfield_code_seen)) {
        if (do_not_abort_on_empty_subfields)
            LOG_WARNING("empty subfield at the end of a " + data_field.getTag().toString() + "-field in the record w/ control number \""
                        + control_number + "\"!");
        else
            LOG_ERROR("empty subfield at the end of a " + data_field.getTag().toString() + "-field in the record w/ control number \""
                      + control_number + "\"!");
    }
}


void CheckLocalBlockConsistency(const MARC::Record &record) {
    auto field(record.begin());

    // Skip to the beginning of the first local block:
    while (field != record.end() and field->getTag().toString() != "LOK")
        ++field;

    // Check the internal structure of each local block:
    while (field != record.end() and field->getTag().toString() == "LOK") {
        if (field->getLocalTag() != "000")
            LOG_ERROR("local block does not start w/ a 000 pseudo tag in the record w/ control number \"" + record.getControlNumber()
                      + "\"!!");
        if (++field == record.end() or field->getLocalTag() != "001")
            LOG_ERROR("local block does not contain a 001 pseudo tag after a 000 pseudo tag in the record w/ control number \""
                      + record.getControlNumber() + "\"!!");

        MARC::Tag last_local_tag;
        while (field != record.end() and field->getTag().toString() == "LOK" and field->getLocalTag() != "000") {
            const MARC::Tag current_local_tag(field->getLocalTag());
            if (unlikely(current_local_tag < last_local_tag))
                LOG_ERROR("invalid tag order in a local block in the record with control number \"" + record.getControlNumber() + "\"!");
            last_local_tag = current_local_tag;
            ++field;
        }
    }
}


void ProcessRecords(const bool do_not_abort_on_empty_subfields, const bool do_not_abort_on_invalid_repeated_fields,
                    const bool check_rule_violations_only, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    const std::vector<Rule *> &rules, File * const rule_violation_list) {
    unsigned record_count(0), control_number_duplicate_count(0), rule_violation_count(0);
    std::unordered_set<std::string> already_seen_control_numbers;

    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        const std::string CONTROL_NUMBER(record.getControlNumber());
        if (unlikely(CONTROL_NUMBER.empty()))
            LOG_ERROR("Record #" + std::to_string(record_count) + " is missing a control number!");

        if (not check_rule_violations_only) {
            if (already_seen_control_numbers.find(CONTROL_NUMBER) == already_seen_control_numbers.end())
                already_seen_control_numbers.emplace(CONTROL_NUMBER);
            else {
                ++control_number_duplicate_count;
                LOG_WARNING("found duplicate control number \"" + CONTROL_NUMBER + "\"!");
            }

            CheckFieldOrder(do_not_abort_on_invalid_repeated_fields, record);

            MARC::Tag last_tag(std::string(MARC::Record::TAG_LENGTH, ' '));
            for (const auto &field : record) {
                if (not field.getTag().isTagOfControlField())
                    CheckDataField(do_not_abort_on_empty_subfields, field, CONTROL_NUMBER);

                if (unlikely(field.getTag() < last_tag))
                    LOG_ERROR("Incorrect non-alphanumeric field order in record w/ control number \"" + CONTROL_NUMBER + "\"!");
                last_tag = field.getTag();
            }

            CheckLocalBlockConsistency(record);
        }

        if (rule_violation_list != nullptr and not rules.empty()) {
            for (const auto rule : rules) {
                std::string err_msg;
                if (rule->hasBeenViolated(record, &err_msg)) {
                    ++rule_violation_count;
                    (*rule_violation_list) << record.getControlNumber() << ": " << err_msg << '\n';
                }
            }
        }

        if (marc_writer != nullptr)
            marc_writer->write(record);
    }

    if (control_number_duplicate_count > 0)
        LOG_ERROR("Found " + std::to_string(control_number_duplicate_count) + " duplicate control numbers!");

    LOG_INFO("Data set contains " + std::to_string(record_count) + " valid MARC record(s) w/ " + std::to_string(rule_violation_count)
             + " rule violations.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    bool do_not_abort_on_empty_subfields(false);
    if (std::strcmp(argv[1], "--do-not-abort-on-empty-subfields") == 0) {
        do_not_abort_on_empty_subfields = true;
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    bool do_not_abort_on_invalid_repeated_fields(false);
    if (std::strcmp(argv[1], "--do-not-abort-on-invalid-repeated-fields") == 0) {
        do_not_abort_on_invalid_repeated_fields = true;
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    bool check_rule_violations_only(false);
    if (std::strcmp(argv[1], "--check-rule-violations-only") == 0) {
        check_rule_violations_only = true;
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    std::string output_filename;
    if (StringUtil::StartsWith(argv[1], "--write-data=")) {
        output_filename = argv[1] + __builtin_strlen("--write-data=");
        --argc, ++argv;
    }

    if (argc != 2 and argc != 4)
        Usage();

    std::vector<Rule *> rules;
    std::unique_ptr<File> rule_violation_list;
    if (argc == 4) {
        LoadRules(argv[2], &rules);
        rule_violation_list = FileUtil::OpenOutputFileOrDie(argv[3]);
    }

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));

    std::unique_ptr<MARC::Writer> marc_writer;
    if (not output_filename.empty())
        marc_writer = MARC::Writer::Factory(output_filename);

    ProcessRecords(do_not_abort_on_empty_subfields, do_not_abort_on_invalid_repeated_fields, check_rule_violations_only, marc_reader.get(),
                   marc_writer.get(), rules, rule_violation_list.get());

    return EXIT_SUCCESS;
}
