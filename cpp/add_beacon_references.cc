/** \brief Utility for deleting partial or entire MARC records based on an input list.
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <iostream>
#include <map>
#include <memory>
#include <cstdlib>
#include "Downloader.h"
#include "File.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"

namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--input-format=(marc-21|marc-xml)] [--output-format=(marc-21|marc-xml)] input_marc21 output_marc21\n";
    std::exit(EXIT_FAILURE);
}


// TODO: add archivportal-d (BEACON url not found)
// BEACON file can only be used if entry elemtents are plain GND numbers.
const std::map<std::string, std::string> beacon_id_to_url_map({ { "kalliope", "http://kalliope.staatsbibliothek-berlin.de/beacon/beacon.txt" } });


std::map<std::string, std::set<std::string>> PopulateGNDToBeaconIdsMap() {
    std::map<std::string, std::set<std::string>> gnd_to_beacon_ids_map;

    FileUtil::AutoTempDirectory temp_dir;
    for (const auto &beacon_id_and_url : beacon_id_to_url_map) {
        const std::string beacon_id(beacon_id_and_url.first);
        const std::string beacon_url(beacon_id_and_url.second);
        const std::string beacon_temp_path(temp_dir.getDirectoryPath() + "/" + beacon_id);

        LOG_INFO("Downloading/Processing " + beacon_id + " BEACON file from " + beacon_url);
        if (not Download(beacon_url, beacon_temp_path, Downloader::DEFAULT_TIME_LIMIT))
            LOG_ERROR("BEACON file could not be downloaded: " + beacon_url);

        File beacon_file(beacon_temp_path, "r");
        unsigned beacon_gnd_count(0);
        while (not beacon_file.eof()) {
            const std::string line(beacon_file.getline());
            if (not StringUtil::StartsWith(line, "#")) {
                const std::string gnd(line);
                ++beacon_gnd_count;
                auto gnd_to_beacon_ids_iter(gnd_to_beacon_ids_map.find(gnd));
                if (gnd_to_beacon_ids_iter == gnd_to_beacon_ids_map.end())
                    gnd_to_beacon_ids_map.emplace(gnd, std::initializer_list<std::string>{ beacon_id });
                else
                    gnd_to_beacon_ids_iter->second.emplace(beacon_id);
            }
        }
        LOG_INFO("Found " + std::to_string(beacon_gnd_count) + " GND numbers in " + beacon_id + " BEACON file.");
    }

    return gnd_to_beacon_ids_map;
}


void ProcessRecords(const std::map<std::string, std::set<std::string>> &gnd_to_beacon_ids_map,
                    MARC::Reader * const marc_reader, MARC::Writer * const marc_writer)
{
    unsigned beacon_reference_count(0);
    while (MARC::Record record = marc_reader->read()) {
        std::string gnd;
        if (MARC::GetGNDCode(record, &gnd)) {
            const auto gnd_to_beacon_ids(gnd_to_beacon_ids_map.find(gnd));
            if (gnd_to_beacon_ids != gnd_to_beacon_ids_map.end()) {
                MARC::Record::Field beacon_field("BEA");
                beacon_field.appendSubfield('a', "1");
                for (const std::string &beacon_id : gnd_to_beacon_ids->second) {
                    beacon_field.appendSubfield('b', beacon_id);
                    ++beacon_reference_count;
                }

                record.insertField(beacon_field);
            }
        }

        marc_writer->write(record);
    }

    LOG_INFO("Added " + std::to_string(beacon_reference_count) + " BEACON references to MARC records!");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 3)
        Usage();

    const auto reader_type(MARC::GetOptionalReaderType(&argc, &argv, 1));
    const auto writer_type(MARC::GetOptionalWriterType(&argc, &argv, 1));

    if (argc != 3)
        Usage();

    const auto marc_reader(MARC::Reader::Factory(argv[1], reader_type));
    const auto marc_writer(MARC::Writer::Factory(argv[2], writer_type));

    std::map<std::string, std::set<std::string>> gnd_to_beacon_ids_map(PopulateGNDToBeaconIdsMap());
    ProcessRecords(gnd_to_beacon_ids_map, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
