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
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--do-not-abort-on-empty-subfields] [--do-not-abort-on-invalid-repeated-fields] "
              << "[--write-data=output_filename] marc_data\n"
              << "       If \"--write-data\" has been specified, the read records will be written out again.\n\n";
    std::exit(EXIT_FAILURE);
}


void CheckFieldOrder(const bool do_not_abort_on_invalid_repeated_fields, const MARC::Record &record) {
    MARC::Tag last_tag;
    for (const auto &field : record) {
        const MARC::Tag current_tag(field.getTag());
        if (unlikely(current_tag < last_tag))
            LOG_ERROR("invalid tag order in the record with control number \"" + record.getControlNumber() + "\" (\""
                      + last_tag.toString() + "\" followed by \"" + current_tag.toString() + "\")!");
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
                    MARC::Reader * const marc_reader, MARC::Writer * const marc_writer)
{
    unsigned record_count(0), control_number_duplicate_count(0);
    std::unordered_set<std::string> already_seen_control_numbers;

    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        const std::string CONTROL_NUMBER(record.getControlNumber());
        if (unlikely(CONTROL_NUMBER.empty()))
            LOG_ERROR("Record #" + std::to_string(record_count) + " is missing a control number!");

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

        if (marc_writer != nullptr)
            marc_writer->write(record);
    }

    if (control_number_duplicate_count > 0)
        LOG_ERROR("Found " + std::to_string(control_number_duplicate_count) + " duplicate control numbers!");
    std::cout << "Data set contains " << record_count << " valid MARC record(s).\n";
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

    std::string output_filename;
    if (StringUtil::StartsWith(argv[1], "--write-data=")) {
        output_filename = argv[1] + __builtin_strlen("--write-data=");
        --argc, ++argv;
    }

    if (argc != 2)
        Usage();

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));

    std::unique_ptr<MARC::Writer> marc_writer;
    if (not output_filename.empty())
        marc_writer = MARC::Writer::Factory(output_filename);

    ProcessRecords(do_not_abort_on_empty_subfields, do_not_abort_on_invalid_repeated_fields, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
