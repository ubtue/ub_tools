/** \file    add_authority_beacon_information.cc
 *  \brief   Adds BEACON information to authority files
 *  \author  Dr. Johannes Ruscheinski
 *
 *  Copyright (C) 2018,2019 Library of the University of Tübingen
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

#include <cstdlib>
#include "BeaconFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "Url.h"
#include "util.h"


namespace {


std::string NameFromURL(const std::string &url_string) {
    const Url url(url_string);
    std::string name(url.getAuthority());
    if (StringUtil::StartsWith(name, "www.", /* ignore_case = */true))
        name = name.substr(__builtin_strlen("www."));
    const auto last_dot_pos(name.rfind('.'));
    if (last_dot_pos != std::string::npos)
        name.resize(last_dot_pos);
    StringUtil::Map(&name, '.', ' ');

    // Convert the first letter of each "word" to uppercase:
    bool first_char_of_word(true);
    for (auto &ch : name) {
        if (first_char_of_word)
            ch = std::toupper(ch);
        first_char_of_word = ch == ' ' or ch == '-';
    }

    return name;
}


void ProcessAuthorityRecords(MARC::Reader * const authority_reader, MARC::Writer * const authority_writer,
                             const std::vector<BeaconFile> &beacon_files)
{
    unsigned gnd_tagged_count(0);
    while (auto record = authority_reader->read()) {
        std::string gnd_number;
        if (MARC::GetGNDCode(record, &gnd_number)) {
            for (const auto &beacon_file : beacon_files) {
                const auto beacon_entry(beacon_file.find(gnd_number));
                if (beacon_entry == beacon_file.end())
                    continue;

                ++gnd_tagged_count;
                record.insertField("BEA",
                                   { { 'a', NameFromURL(beacon_file.getUrlTemplate()) }, { 'u', beacon_file.getURL(*beacon_entry) } });
            }
        }

        authority_writer->write(record);
    }

    LOG_INFO("tagged " + std::to_string(gnd_tagged_count) + " author records with beacon links.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 4)
        ::Usage("authority_records augmented_authority_records [beacon_list1 beacon_list2 .. beacon_listN]");

    const std::string authority_records_filename(argv[1]);
    const std::string augmented_authority_records_filename(argv[2]);

    if (unlikely(authority_records_filename == augmented_authority_records_filename))
        LOG_ERROR("Authority data input file name equals authority output file name!");

    auto authority_reader(MARC::Reader::Factory(authority_records_filename));
    auto authority_writer(MARC::Writer::Factory(augmented_authority_records_filename));

    std::vector<BeaconFile> beacon_files;
    for (int arg_no(3); arg_no < argc; ++arg_no)
        beacon_files.emplace_back(BeaconFile(argv[arg_no]));

    ProcessAuthorityRecords(authority_reader.get(), authority_writer.get(), beacon_files);

    return EXIT_SUCCESS;
}
