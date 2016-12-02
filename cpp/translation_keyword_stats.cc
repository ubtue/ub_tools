/** \file translation_keyword_stats.cc
 *  \brief A tool for generating some stats for Martin Faßnacht.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "MarcReader.h"
#include "MarcRecord.h"
#include "Subfields.h"
#include "util.h"


void GetSystemCodes(const MarcRecord &record, std::vector<std::string> * const system_codes) {
    std::vector<size_t> _065_field_indices;
    record.getFieldIndices("065", &_065_field_indices);
    for (const size_t _065_index : _065_field_indices) {
        const Subfields _065_subfields(record.getFieldData(_065_index));
        const std::string _2_contents(_065_subfields.getFirstSubfieldValue('2'));
        if (_2_contents != "sswd")
            continue;
        const std::string a_contents(_065_subfields.getFirstSubfieldValue('a'));
        if (likely(not a_contents.empty()))
            system_codes->push_back(a_contents);
    }
}


void GenerateStats(MarcReader * const marc_reader, File * const _150a_output, File * const _450a_output) {
    while (const MarcRecord record = marc_reader->read()) {
        const size_t _150_index(record.getFieldIndex("150"));
        if (_150_index == MarcRecord::FIELD_NOT_FOUND)
            continue;

        const Subfields _150_subfields(record.getFieldData(_150_index));
        const std::string _150a_contents(_150_subfields.getFirstSubfieldValue('a'));
        if (unlikely(_150a_contents.empty()))
            continue;

        const std::string ppn(record.getControlNumber());
        (*_150a_output) << ppn << ':' << _150a_contents;

        std::vector<std::string> system_codes;
        GetSystemCodes(record, &system_codes);
        for (const auto &system_code : system_codes)
            (*_150a_output) << ',' << system_code;
        
        (*_150a_output) << '\n';

        std::vector<size_t> _450_field_indices;
        record.getFieldIndices("450", &_450_field_indices);
        for (const size_t _450_field_index : _450_field_indices) {
            const Subfields _450_subfields(record.getFieldData(_450_field_index));
            const std::string _450a_contents(_450_subfields.getFirstSubfieldValue('a'));
            if (likely(not _450a_contents.empty()))
                (*_450a_output) << ppn << ':' << _450a_contents << '\n';
        }
    }
}


int main(int argc, char *argv[]) {
    if (argc != 2)
        Error("Usage: " + std::string(argv[0]) + " marc_authority_filename");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));
    std::unique_ptr<File> _150a_output(FileUtil::OpenOutputFileOrDie("150a"));
    std::unique_ptr<File> _450a_output(FileUtil::OpenOutputFileOrDie("450a"));
    
    try {
        GenerateStats(marc_reader.get(), _150a_output.get(), _450a_output.get());
    } catch(const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
