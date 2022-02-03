/** \brief Utility for filtering out MARC records based on alphanumeric ranges.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


struct Range {
    std::string start_, end_;

public:
    Range() = default;
    Range(const Range &other) = default;
    Range(const std::string &start, const std::string &end): start_(start), end_(end) { }

    Range &operator=(const Range &lhs) = default;
};


void ParseRanges(const std::string &raw_ranges, std::vector<Range> * const parsed_ranges) {
    std::vector<std::string> ranges;
    StringUtil::Split(raw_ranges, '|', &ranges);
    for (const auto &range : ranges) {
        const auto dash_pos(range.find('-'));
        if (dash_pos == std::string::npos)
            LOG_ERROR("bad range is missing a dash: \"" + range + "\"!");
        const std::string start(range.substr(0, dash_pos));
        const std::string end(range.substr(dash_pos + 1));
        if (start >= end)
            LOG_ERROR("the range end must be follow the range start: \"" + range + "\"!");
        parsed_ranges->emplace_back(start, end);
    }
}


void ProcessRecords(const std::string &field_and_subfield_code, const std::vector<Range> &ranges, MARC::Reader * const marc_reader,
                    MARC::Writer * const marc_writer) {
    unsigned record_count(0), dropped_record_count(0);

next_record:
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        for (const auto &field : record.getTagRange(field_and_subfield_code.substr(0, MARC::Record::TAG_LENGTH))) {
            const MARC::Subfields subfields(field.getSubfields());
            for (const auto &subfield : subfields) {
                if (subfield.code_ == field_and_subfield_code[MARC::Record::TAG_LENGTH]) {
                    for (const auto &range : ranges) {
                        if (subfield.value_ < range.start_ or subfield.value_ > range.end_) {
                            ++dropped_record_count;
                            goto next_record;
                        }
                    }
                }
            }
        }

        marc_writer->write(record);
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " record(s) and dropped " + std::to_string(dropped_record_count)
             + " record(s).");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 5)
        ::Usage(
            "ranges field_and_subfield_code marc_input marc_output\n"
            "ranges is a list of ranges separated by vertical bars.  An example range would be A123-A297, an example\n"
            "field_and_subfield_code might be \"015a\" indicating field 015 and subfield code a.");

    std::vector<Range> ranges;
    ParseRanges(argv[1], &ranges);

    const std::string field_and_subfield_code(argv[2]);
    if (field_and_subfield_code.length() != MARC::Record::TAG_LENGTH + 1)
        LOG_ERROR("bad field_and_subfield_code: \"" + field_and_subfield_code + "\"!");

    const auto marc_reader(MARC::Reader::Factory(argv[3]));
    const auto marc_writer(MARC::Writer::Factory(argv[4]));
    ProcessRecords(field_and_subfield_code, ranges, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
