/** \file    convert_json_to_marc.cc
 *  \brief   Generates a filoew needed by convert_json_to_marc.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2020 Library of the University of Tübingen

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

#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


namespace {


std::string EscapeColons(const std::string &unescaped_string) {
    std::string escaped_string;
    escaped_string.reserve(unescaped_string.size());

    for (const char ch : unescaped_string) {
        if (ch == ':' or ch == '\\')
            escaped_string += '\\';
        escaped_string += ch;
    }

    return escaped_string;
}


void ProcessRecords(MARC::Reader * const reader, File * const output) {
    unsigned processed_count(0), generated_count(0);

    while (const auto record = reader->read()) {
        ++processed_count;

        if (not record.isSerial())
            continue;

        const auto issns(record.getISSNs());
        if (issns.empty())
            continue;

        const auto title(record.getMainTitle());
        if (title.empty())
            continue;

        const auto ppn(record.getControlNumber());
        for (const auto issn : issns) {
            (*output) << issn << ':' << EscapeColons(title) << ':' << ppn << '\n';
            ++generated_count;
        }
    }

    LOG_INFO("Processed " + std::to_string(processed_count) + " MARC records and generated " + std::to_string(generated_count) + " map entry/entries.");
}


} // namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        ::Usage("marc_input mapfile_output");

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto output(FileUtil::OpenOutputFileOrDie(argv[2]));

    ProcessRecords(marc_reader.get(), output.get());

    return EXIT_SUCCESS;
}
