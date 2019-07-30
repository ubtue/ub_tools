/** \file    create_literary_remains_records.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for creating literary remains MARC records from Beacon files.
 */

/*
    Copyright (C) 2019, Library of the University of Tübingen

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

#include <unordered_map>
#include <unordered_set>
#include "BeaconFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


void CopyMarc(MARC::Reader * const reader, MARC::Writer * const writer) {
    while (auto record = reader->read())
        writer->write(record);
}


void LoadAuthorGNDNumbers(const std::string &filename, std::unordered_set<std::string> * const author_gnd_numbers,
                          std::unordered_map<std::string, std::string> * const gnd_numbers_to_author_names_map)
{
    auto reader(MARC::Reader::Factory(filename));

    unsigned total_count(0);
    while (auto record = reader->read()) {
        ++total_count;

        const auto _100_field(record.findTag("100"));
        if (_100_field == record.end() or not _100_field->hasSubfield('a'))
            continue;

        const std::string author_name(_100_field->getFirstSubfieldWithCode('a'));
        std::string gnd_number;
        if (MARC::GetGNDCode(record, &gnd_number)) {
            author_gnd_numbers->emplace(gnd_number);
            (*gnd_numbers_to_author_names_map)[gnd_number] = author_name;
        }
    }

    LOG_INFO("Loaded " + std::to_string(author_gnd_numbers->size()) + " author GND numbers from \"" + filename
             + "\" which contained a total of " + std::to_string(total_count) + " records.");
}


void AppendLiteraryRemainsRecords(MARC::Writer * const writer, const BeaconFile &beacon_file,
                                  const std::unordered_set<std::string> &author_gnd_numbers,
                                  const std::unordered_map<std::string, std::string> &gnd_numbers_to_author_names_map)
{
    static char infix = 'A' - 1;
    ++infix;

    unsigned creation_count(0);
    for (const auto &beacon_entry : beacon_file) {
        if (author_gnd_numbers.find(beacon_entry.gnd_number_) != author_gnd_numbers.cend()) {
            ++creation_count;

            MARC::Record new_record(MARC::Record::TypeOfRecord::MIXED_MATERIALS, MARC::Record::BibliographicLevel::COLLECTION,
                                    "LR" + std::string(1, infix) + StringUtil::ToString(creation_count, 10, 6));
            const std::string &author_name(gnd_numbers_to_author_names_map.find(beacon_entry.gnd_number_)->second);
            new_record.insertField("100", { { 'a', author_name }, { '0', "(DE-588)" + beacon_entry.gnd_number_ } });
            new_record.insertField("245", { { 'a', "Nachlass von " + author_name } });
            new_record.insertField("856", { { 'u', beacon_file.getURL(beacon_entry) }, { '3', "Nachlassdatenbank" } });
            writer->write(new_record);
        }
    }

    LOG_INFO("Created " + std::to_string(creation_count) + " record(s) for \"" + beacon_file.getFileName() + "\".");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 5)
        ::Usage("marc_input marc_output authority_records beacon_file1 [beacon_file2 .. beacon_fileN]");

    auto reader(MARC::Reader::Factory(argv[1]));
    auto writer(MARC::Writer::Factory(argv[2]));
    CopyMarc(reader.get(), writer.get());

    std::unordered_set<std::string> author_gnd_numbers;
    std::unordered_map<std::string, std::string> gnd_numbers_to_author_names_map;
    LoadAuthorGNDNumbers(argv[3], &author_gnd_numbers, &gnd_numbers_to_author_names_map);

    for (int arg_no(4); arg_no < argc; ++arg_no) {
        const BeaconFile beacon_file(argv[arg_no]);
        LOG_INFO("Loaded " + std::to_string(beacon_file.size()) + " entries from \"" + beacon_file.getFileName() + "\"!");
        AppendLiteraryRemainsRecords(writer.get(), beacon_file, author_gnd_numbers, gnd_numbers_to_author_names_map);
    }

    return EXIT_SUCCESS;
}
