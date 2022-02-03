/** \file translation_keyword_stats.cc
 *  \brief A tool for generating some stats for Martin Faßnacht.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include "Compiler.h"
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


namespace {


void GetSystemCodes(const MARC::Record &record, std::vector<std::string> * const system_codes) {
    for (const auto &_065_field : record.getTagRange("065")) {
        const MARC::Subfields _065_subfields(_065_field.getSubfields());
        const std::string _2_contents(_065_subfields.getFirstSubfieldWithCode('2'));
        if (_2_contents != "sswd")
            continue;
        const std::string a_contents(_065_subfields.getFirstSubfieldWithCode('a'));
        if (likely(not a_contents.empty()))
            system_codes->push_back(a_contents);
    }
}


void GenerateStats(MARC::Reader * const marc_reader, File * const _150a_output, File * const _450a_output) {
    while (const MARC::Record record = marc_reader->read()) {
        const auto _150_field(record.getFirstField("150"));
        if (_150_field == record.end())
            continue;

        const MARC::Subfields _150_subfields(_150_field->getSubfields());
        const std::string _150a_contents(_150_subfields.getFirstSubfieldWithCode('a'));
        if (unlikely(_150a_contents.empty()))
            continue;

        const std::string ppn(record.getControlNumber());
        (*_150a_output) << ppn << ':' << _150a_contents;

        std::vector<std::string> system_codes;
        GetSystemCodes(record, &system_codes);
        for (const auto &system_code : system_codes)
            (*_150a_output) << ',' << system_code;

        (*_150a_output) << '\n';

        for (const auto &_450_field : record.getTagRange("450")) {
            const MARC::Subfields _450_subfields(_450_field.getSubfields());
            const std::string _450a_contents(_450_subfields.getFirstSubfieldWithCode('a'));
            if (likely(not _450a_contents.empty()))
                (*_450a_output) << ppn << ':' << _450a_contents << '\n';
        }
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        logger->error("Usage: " + std::string(argv[0]) + " marc_authority_filename");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<File> _150a_output(FileUtil::OpenOutputFileOrDie("150a"));
    std::unique_ptr<File> _450a_output(FileUtil::OpenOutputFileOrDie("450a"));

    GenerateStats(marc_reader.get(), _150a_output.get(), _450a_output.get());

    return EXIT_SUCCESS;
}
