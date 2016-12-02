/** \file    update_ixtheo_notations.cc
 *  \brief   Move the ixTheo classification notations from local data into field 652a.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015, Library of the University of Tübingen

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
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcRecord.h"
#include "MarcReader.h"
#include "MarcWriter.h"
#include "MarcXmlWriter.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output code_to_description_map\n";
    std::exit(EXIT_FAILURE);
}


void LoadCodeToDescriptionMap(File * const code_to_description_map_file,
                              std::unordered_map<std::string, std::string> * const code_to_description_map)
{
    unsigned line_no(0);
    while (not code_to_description_map_file->eof()) {
        const std::string line(code_to_description_map_file->getline());
        ++line_no;
        if (line.length() < 4) // Need at least a 2 character code, a comma and some text.
            continue;

        const size_t comma_pos(line.find(','));
        if (comma_pos == std::string::npos)
            Error("malformed line " + std::to_string(line_no) + " in \"" + code_to_description_map_file->getPath() + "\"! (1)");

        const std::string code(line.substr(0, comma_pos));
        if (code.length() != 2 and code.length() != 3)
            Error("malformed line " + std::to_string(line_no) + " in \"" + code_to_description_map_file->getPath() + "\"! (2)");

        (*code_to_description_map)[code] = line.substr(comma_pos + 1);
    }

    std::cerr << "Found " << code_to_description_map->size() << " code to description mappings.\n";
}


bool LocalBlockIsFromUbTueTheologians(const std::pair<size_t, size_t> &local_block_begin_and_end,
                                      const MarcRecord &record)
{
    std::vector<size_t> _852_indices;
    record.findFieldsInLocalBlock("852", "  ", local_block_begin_and_end, &_852_indices);

    for (const auto index : _852_indices) {
        const Subfields subfields(record.getSubfields(index));
        if (subfields.hasSubfieldWithValue('a', "Tü 135"))
            return true;
    }

    return false;
}


unsigned ExtractIxTheoNotations(const std::pair<size_t, size_t> &local_block_begin_and_end,
                                const MarcRecord &record,
                                const std::unordered_map<std::string, std::string> &code_to_description_map,
                                std::string * const ixtheo_notations_list)
{
    std::vector<size_t> _936ln_indices;
    record.findFieldsInLocalBlock("936", "ln", local_block_begin_and_end, &_936ln_indices);

    size_t found_count(0);
    for (const auto index : _936ln_indices) {
        const Subfields subfields(record.getSubfields(index));
        const std::string ixtheo_notation_candidate(subfields.getFirstSubfieldValue('a'));
        if (code_to_description_map.find(ixtheo_notation_candidate) != code_to_description_map.end()) {
            ++found_count;
            if (ixtheo_notations_list->empty())
                *ixtheo_notations_list = ixtheo_notation_candidate;
            else
                *ixtheo_notations_list += ":" + ixtheo_notation_candidate;
        }
    }

    return found_count;
}


void ProcessRecords(MarcReader * const marc_reader, MarcWriter * const marc_writer,
                    const std::unordered_map<std::string, std::string> &code_to_description_map)
{
    unsigned count(0), ixtheo_notation_count(0), records_with_ixtheo_notations(0);
    while (MarcRecord record = marc_reader->read()) {
        ++count;

        std::vector<std::pair<size_t, size_t>> local_block_boundaries;
        if (record.findAllLocalDataBlocks(&local_block_boundaries) == 0) {
            marc_writer->write(record);
            continue;
        }

        std::string ixtheo_notations_list; // Colon-separated list of ixTheo notations.
        for (const auto &local_block_begin_and_end : local_block_boundaries) {
            if (not LocalBlockIsFromUbTueTheologians(local_block_begin_and_end, record))
                continue;

            const unsigned notation_count(ExtractIxTheoNotations(local_block_begin_and_end, record,
                                                                 code_to_description_map, &ixtheo_notations_list));
            if (notation_count > 0) {
                ++records_with_ixtheo_notations;
                ixtheo_notation_count += notation_count;
            }
        }

        if (not ixtheo_notations_list.empty()) // Insert a new 652 field w/ a $a subfield.
            record.insertSubfield("652", 'a', ixtheo_notations_list);
        marc_writer->write(record);
    }

    std::cerr << "Read " << count << " records.\n";
    std::cerr << records_with_ixtheo_notations << " records had ixTheo notations.\n";
    std::cerr << "Found " << ixtheo_notation_count << " ixTheo notations overall.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1], MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(argv[2], MarcWriter::BINARY));

    const std::string code_to_description_map_filename(argv[3]);
    File code_to_description_map_file(code_to_description_map_filename, "r");
    if (not code_to_description_map_file)
        Error("can't open \"" + code_to_description_map_filename + "\" for reading!");

    try {
        std::unordered_map<std::string, std::string> code_to_description_map;
        LoadCodeToDescriptionMap(&code_to_description_map_file, &code_to_description_map);
        ProcessRecords(marc_reader.get(), marc_writer.get(), code_to_description_map);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
