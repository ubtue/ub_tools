/** \file    find_tue_dups.cc
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2017, Library of the University of Tübingen

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
#include <set>
#include <cstring>
#include "Compiler.h"
#include "TextUtil.h"
#include "MarcRecord.h"
#include "RegexMatcher.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname
              << " --input-format=(BSZ|UB_FREIBURG) --output-set=(ALL|MONOGRAPHS|SERIALS) marc_input\n";
    std::exit(EXIT_FAILURE);
}


void ExtractSubfieldA(const MarcRecord &record, const std::string &tag,
                      std::set<std::string> * const extracted_values)
{
    std::vector<size_t> field_indices;
    record.getFieldIndices(tag, &field_indices);
    for (const size_t field_index : field_indices) {
        const std::string subfield_a(record.extractFirstSubfield(field_index, 'a'));
        if (likely(not subfield_a.empty()))
            extracted_values->emplace(subfield_a);
    }
}


void ExtractISSNsAndISBNs(const MarcRecord &record, std::set<std::string> * const issns_and_isbns) {
    ExtractSubfieldA(record, "022", issns_and_isbns);
    ExtractSubfieldA(record, "020", issns_and_isbns);
}


static const RegexMatcher * const tue_sigil_matcher(RegexMatcher::RegexMatcherFactory("^DE-21.*"));


bool FindTueSigil(const MarcRecord &record, const std::pair<size_t, size_t> &block_start_and_end,
                  std::string * const sigil)
{
    std::vector<size_t> field_indices;
    record.findFieldsInLocalBlock("852", "??", block_start_and_end, &field_indices);

    for (size_t field_index : field_indices) {
        const std::string field_data(record.getFieldData(field_index));
        const Subfields subfields(field_data);
        if (subfields.extractSubfieldWithPattern('a', *tue_sigil_matcher, sigil))
            return true;
    }
    return false;
}


enum InputFormat { BSZ, UB_FREIBURG };


bool FindTueDups(const InputFormat input_format, const char bibliographic_level, const MarcRecord &record) {
    std::vector<std::string> sigils;
    if (input_format == BSZ) {
        std::vector<std::pair<size_t, size_t>> local_block_boundaries;
        ssize_t local_data_count = record.findAllLocalDataBlocks(&local_block_boundaries);
        if (local_data_count == 0)
            return false;

        for (const auto &block_start_and_end : local_block_boundaries) {
            std::string sigil;
            if (FindTueSigil(record, block_start_and_end, &sigil))
                sigils.emplace_back(sigil);
        }
    } else { // input_format == UB_FREIBURG
        std::vector<size_t> _910_indices;
        record.getFieldIndices("910", &_910_indices);
        for (const size_t _910_index : _910_indices) {
            const std::string _910_field_contents(record.getFieldData(_910_index));
            if (_910_field_contents.empty())
                continue;
            const Subfields subfields(_910_field_contents);
            const std::string sigil(subfields.getFirstSubfieldValue('c'));
            if (not sigil.empty())
                sigils.emplace_back(sigil);
        }
    }

    // We only keep dups and only those that occur in the Tübingen University's main library:
    if (sigils.size() < 2 or std::find(sigils.cbegin(), sigils.cend(), "21") == sigils.cend())
        return false;

    std::sort(sigils.begin(), sigils.end());

    const std::string _008_contents(record.getFieldData("008"));
    std::string publication_year;
    if (likely(_008_contents.length() >= 11))
        publication_year = _008_contents.substr(7, 4);

    std::string area;
    const std::string _910_contents(record.getFieldData("910"));
    if (not _910_contents.empty()) {
        const Subfields subfields(_910_contents);
        area = subfields.getFirstSubfieldValue('j');
    }

    const std::string _245_contents(record.getFieldData("245"));
    std::string main_title;
    if (not _245_contents.empty()) {
        const Subfields subfields(_245_contents);
        main_title = subfields.getFirstSubfieldValue('a');
    }

    std::set<std::string> issns_and_isbns;
    ExtractISSNsAndISBNs(record, &issns_and_isbns);

    const std::string bibliographic_level_output(
        bibliographic_level == ' ' ? "" : "\",\"" + std::string(1, bibliographic_level));
    std::cout << '"' << record.getControlNumber() << bibliographic_level_output << "\",\"" << publication_year
              << "\",\"" << StringUtil::Join(issns_and_isbns, ',') <<"\",\"" << area <<"\",\""
              << TextUtil::CSVEscape(main_title) << "\",\"" << StringUtil::Join(sigils, ',') << "\"\n";

    return true;
}


enum OutputSet { ALL, MONOGRAPHS, SERIALS };


void FindTueDups(const InputFormat input_format, const OutputSet output_set, MarcReader * const marc_reader) {
    unsigned count(0), dups_count(0), monograph_count(0), serial_count(0);
    while (MarcRecord record = marc_reader->read()) {
        ++count;

        // Only consider monographs and serials:
        const Leader &leader(record.getLeader());
        if (not (leader.isMonograph() or leader.isSerial())
            or (leader.isMonograph() and output_set == SERIALS)
            or (leader.isSerial() and output_set == MONOGRAPHS))
            continue;

        if (FindTueDups(input_format, output_set == ALL ? leader.getBibliographicLevel() : ' ', record)) {
            ++dups_count;
            if (leader.isMonograph())
                ++monograph_count;
            else
                ++serial_count;
        }
    }
    std::cerr << "Processed " << count << " records and found " << dups_count << " dups (" << monograph_count
              << " monographs and " << serial_count << " serials).\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    InputFormat input_format;
    if (std::strcmp(argv[1], "--input-format=BSZ") == 0)
        input_format = BSZ;
    else if (std::strcmp(argv[1], "--input-format=UB_FREIBURG") == 0)
        input_format = UB_FREIBURG;
    else
        Error("invalid input format \"" + std::string(argv[1]) + "\"!  (Must be either BSZ or UB_FREIBURG)");

    OutputSet output_set;
    if (std::strcmp(argv[2], "--output-set=ALL") == 0)
        output_set = ALL;
    else if (std::strcmp(argv[2], "--output-set=MONOGRAPHS") == 0)
        output_set = MONOGRAPHS;
    else if (std::strcmp(argv[2], "--output-set=SERIALS") == 0)
        output_set = SERIALS;
    else
        Error("invalid input format \"" + std::string(argv[2]) + "\"!  (Must be ALL, MONOGRAPHS or SERIALS)");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[3], MarcReader::BINARY));

    try {
        FindTueDups(input_format, output_set, marc_reader.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
