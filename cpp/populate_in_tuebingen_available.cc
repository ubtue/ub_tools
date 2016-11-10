/** \file    populate_in_tuebingen_available.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool that adds a new "SIG" field to a MARC record if there are UB or IFK call numbers in a record.
 */

/*
    Copyright (C) 2015,2016, Library of the University of Tübingen

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

#include <iostream>
#include <limits>
#include <set>
#include <cstdlib>
#include "Compiler.h"
#include "DirectoryEntry.h"
#include "HtmlUtil.h"
#include "Leader.h"
#include "MarcRecord.h"
#include "MarcReader.h"
#include "MarcWriter.h"
#include "MarcXmlWriter.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


class Range {
public:
    static const unsigned INFINITE_VOLUME = std::numeric_limits<unsigned>::max();
    static const unsigned INFINITE_YEAR = std::numeric_limits<unsigned>::max();
public:
    unsigned start_volume_;
    unsigned start_year_;
    unsigned end_volume_;
    unsigned end_year_;
public:
    Range(const unsigned start_volume, const unsigned start_year, const unsigned end_volume, const unsigned end_year)
        : start_volume_(start_volume), start_year_(start_year), end_volume_(end_volume), end_year_(end_year) { }

    inline bool inRange(const unsigned volume, const unsigned year) const {
        return start_volume_ < volume < end_volume_ and start_year_ < year < end_year_;
    }
};


static unsigned modified_record_count;
static unsigned add_sig_count;


// Returns UB and criminology sigils or the empty string.
std::string FindSigil(MarcRecord * const record, const std::pair<size_t, size_t> &block_start_and_end) {
    std::vector<size_t> field_indices;
    record->findFieldsInLocalBlock("852", "  ", block_start_and_end, &field_indices);
    for (size_t field_index : field_indices) {
        const std::string _852a_contents(record->extractFirstSubfield(field_index, 'a'));
        if (StringUtil::StartsWith(_852a_contents, "DE-21"))
            return _852a_contents;
    }

    return "";
}


bool ParseRanges1(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
        "^\\s*(\\d+)\\.(\\d{4})\\s*-\\s*(\\d+)\\.(\\d{4})\\s*$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            Warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            Warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_volume;
        if (not StringUtil::ToUnsigned((*matcher)[3], &end_volume)) {
            Warning("can't convert \"" + (*matcher)[3] + "\" to an unsigned end volume!");
            continue;
        }

        unsigned end_year;
        if (not StringUtil::ToUnsigned((*matcher)[4], &end_year)) {
            Warning("can't convert \"" + (*matcher)[4] + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, end_volume, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges2(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^\\s*(\\d{4})\\.(\\d{4})\\s*-\\s*$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_volume;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_volume)) {
            Warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start volume!");
            continue;
        }

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &start_year)) {
            Warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned start year!");
            continue;
        }

        ranges->emplace_back(Range(start_volume, start_year, Range::INFINITE_VOLUME, Range::INFINITE_YEAR));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges3(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^\\s*(\\d{4})\\s*-\\s*(\\d{4})\\s*$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            Warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        unsigned end_year;
        if (not StringUtil::ToUnsigned((*matcher)[2], &end_year)) {
            Warning("can't convert \"" + (*matcher)[2] + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(Range::INFINITE_VOLUME, start_year, Range::INFINITE_VOLUME, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges5(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^\\s*(\\d{4})/(\\d{2})\\s*$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            Warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        const std::string end_year_string((*matcher)[1].substr(0, 2) + (*matcher)[2]);
        unsigned end_year;
        if (not StringUtil::ToUnsigned(end_year_string, &end_year)) {
            Warning("can't convert \"" + end_year_string + "\" to an unsigned end year!");
            continue;
        }

        ranges->emplace_back(Range(Range::INFINITE_VOLUME, start_year, Range::INFINITE_VOLUME, end_year));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


bool ParseRanges4(const std::vector<std::string> &individual_ranges, std::vector<Range> * const ranges) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^\\s*(\\d{4})\\s*$"));
    bool found_at_least_one_match(false);
    for (const auto &individual_range : individual_ranges) {
        if (not matcher->matched(individual_range))
            continue;

        unsigned start_year;
        if (not StringUtil::ToUnsigned((*matcher)[1], &start_year)) {
            Warning("can't convert \"" + (*matcher)[1] + "\" to an unsigned start year!");
            continue;
        }

        ranges->emplace_back(Range(Range::INFINITE_VOLUME, start_year, Range::INFINITE_VOLUME, Range::INFINITE_YEAR));
        found_at_least_one_match = true;
    }

    return found_at_least_one_match;
}


void ParseRanges(const std::string &_866a_contents, std::vector<Range> * const ranges) {
    ranges->clear();
    std::vector<std::string> individual_ranges;
    StringUtil::SplitThenTrimWhite(_866a_contents, ';', &individual_ranges);
    if (ParseRanges1(individual_ranges, ranges))
        return;
    if (ParseRanges2(individual_ranges, ranges))
        return;
    if (ParseRanges3(individual_ranges, ranges))
        return;
    if (ParseRanges4(individual_ranges, ranges))
        return;
    if (ParseRanges5(individual_ranges, ranges))
        return;

    for (const auto &individual_range : individual_ranges)
        Warning("couldn't match range: \"" + individual_range + "\"!");
}


bool ProcessSerialRecord(MarcRecord * const record, MarcWriter * const /*output*/, std::string * const /*err_msg*/) {
    if (not record->getLeader().isSerial())
        return true;

    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    record->findAllLocalDataBlocks(&local_block_boundaries);
    for (const auto &block_start_and_end : local_block_boundaries) {
        const std::string sigil(FindSigil(record, block_start_and_end));
        if (sigil != "DE-21" and sigil != "DE-21-110")
            continue;

        std::vector<size_t> field_indices;
        record->findFieldsInLocalBlock("866", "30", block_start_and_end, &field_indices);
        if (field_indices.empty())
            continue;

        for (size_t field_index : field_indices) {
            const std::string _866a_contents(record->extractFirstSubfield(field_index, 'a'));
            if (unlikely(_866a_contents.empty()))
                continue;

            std::vector<Range> ranges;
            ParseRanges(_866a_contents, &ranges);
        }
    }

    return true;
}


bool ProcessRecord(MarcRecord * const record, MarcWriter * const marc_writer, std::string * const /*err_msg*/) {
    std::vector <std::pair<size_t, size_t>> local_block_boundaries;
    record->findAllLocalDataBlocks(&local_block_boundaries);

    bool modified_record(false);
    std::set<std::string> alread_seen_urls;
    for (const auto &block_start_and_end : local_block_boundaries) {
        std::vector <size_t> _852_field_indices;
        if (record->findFieldsInLocalBlock("852", "??", block_start_and_end, &_852_field_indices) == 0)
            continue;
        for (const size_t _852_index : _852_field_indices) {
            const Subfields subfields1(record->getSubfields(_852_index));
            const std::string not_available_subfield(subfields1.getFirstSubfieldValue('z'));
            if (not_available_subfield == "Kein Bestand am IfK; Nachweis für KrimDok")
                continue;

            const std::string isil_subfield(subfields1.getFirstSubfieldValue('a'));
            if (isil_subfield != "DE-21" and isil_subfield != "DE-21-110")
                continue;

            std::string detailed_availability;
            std::vector <size_t> _866_field_indices;
            if (record->findFieldsInLocalBlock("866", "30", block_start_and_end, &_866_field_indices) > 0) {
                for (const size_t _866_index : _866_field_indices) {
                    const Subfields subfields2(record->getSubfields(_866_index));
                    const std::string subfield_a(subfields2.getFirstSubfieldValue('a'));
                    if (not subfield_a.empty()) {
                        if (not detailed_availability.empty())
                            detailed_availability += "; ";
                        detailed_availability += subfield_a;
                        const std::string subfield_z(subfields2.getFirstSubfieldValue('z'));
                        if (not subfield_z.empty())
                            detailed_availability += " " + subfield_z;
                    }
                }
            }

            const std::string institution(isil_subfield == "DE-21" ? "UB: " : "IFK: ");
            if (_852_index + 1 < block_start_and_end.second) {
                const Subfields subfields2(record->getSubfields(_852_index + 1));
                const std::string call_number_subfield(subfields2.getFirstSubfieldValue('c'));
                if (not call_number_subfield.empty()) {
                    const std::string institution_and_call_number(institution + call_number_subfield);
                    ++add_sig_count;
                    modified_record = true;
                    record->insertSubfield("SIG", 'a', institution_and_call_number
                                           + (detailed_availability.empty() ? "" : "(" + detailed_availability
                                              + ")"));
                } else { // Look for a URL.
                    std::vector <size_t> _856_field_indices;
                    if (record->findFieldsInLocalBlock("856", "4 ", block_start_and_end, &_856_field_indices) > 0) {
                        const Subfields subfields3(record->getSubfields(_856_field_indices.front()));
                        if (subfields3.hasSubfield('u')) {
                            const std::string url(subfields3.getFirstSubfieldValue('u'));
                            if (alread_seen_urls.find(url) == alread_seen_urls.cend()) {
                                alread_seen_urls.insert(url);
                                std::string anchor(HtmlUtil::HtmlEscape(subfields3.getFirstSubfieldValue('x')));
                                if (anchor.empty())
                                    anchor = "Tübingen Online Resource";
                                record->insertSubfield("SIG", 'a', "<a href=\"" + url + "\">" + anchor + "</a>");
                            }
                        }
                    }
                }
            }
            break;
        }
    }

    if (modified_record)
        ++modified_record_count;

    marc_writer->write(*record);
    return true;
}


void PopulateTheInTuebingenAvailableField(const bool verbose, MarcReader * const marc_reader,
                                          MarcWriter * const marc_writer)
{
    std::string err_msg;
    if (not MarcRecord::ProcessRecords(marc_reader, ProcessSerialRecord, marc_writer, &err_msg))
        Error("error while processing serial records: " + err_msg);

    if (not MarcRecord::ProcessRecords(marc_reader, ProcessRecord, marc_writer, &err_msg))
        Error("error while processing records: " + err_msg);

    if (verbose) {
        std::cout << "Modified " << modified_record_count << " records.\n";
        std::cout << "Added " << add_sig_count << " signature fields.\n";
    }
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3 and argc != 4)
        Usage();

    bool verbose;
    if (argc == 3)
        verbose = false;
    else { // argc == 4
        if (std::strcmp(argv[1], "--verbose") != 0)
            Usage();
        verbose = true;
    }

    try {
        std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[argc == 3 ? 1 : 2]));
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(argv[argc == 3 ? 2 : 3], MarcWriter::BINARY));
        PopulateTheInTuebingenAvailableField(verbose, marc_reader.get(), marc_writer.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
