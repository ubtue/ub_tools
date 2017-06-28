/** \file    subfield_code_replacer.cc
 *  \brief   A tool for replacing subfield codes in MARC-21 data sets.
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

#include <iostream>
#include <memory>
#include <cstdlib>
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--input-format=(marc-xml|marc-21)] marc_input marc_output pattern1 ";
    std::cerr << "[pattern2 .. patternN]\n";
    std::cerr << "  where each pattern must look like TTTa=b where TTT is a tag and \"a\" and \"b\"\n";
    std::cerr << "  are subfield codes.\n\n";
    std::exit(EXIT_FAILURE);
}


struct Replacement {
    std::string tag_;
    char old_code_, new_code_;
public:
    Replacement(const std::string &tag, const char old_code, const char new_code)
        : tag_(tag), old_code_(old_code), new_code_(new_code) { }
};


// \return True if at least one code has been replaced else false.
bool ReplaceCodes(MarcRecord * const record, const std::vector<Replacement> &replacements) {
    bool replaced_at_least_one_code(false);

    for (const auto &replacement : replacements) {
        std::vector<size_t> field_indices;
        if (record->getFieldIndices(replacement.tag_, &field_indices) == 0)
            continue;

        for (const size_t field_index : field_indices) {
            Subfields subfields(record->getFieldData(field_index));
            if (subfields.replaceSubfieldCode(replacement.old_code_, replacement.new_code_)) {
                replaced_at_least_one_code = true;
                record->updateField(field_index, subfields.toString());
            }
        }
    }

    return replaced_at_least_one_code;
}


void ReplaceCodes(MarcReader * const marc_reader, MarcWriter * const marc_writer,
                  const std::vector<Replacement> &replacements)
{
    unsigned total_count(0), modified_count(0);

    while (MarcRecord record = marc_reader->read()) {
        ++total_count;

        if (ReplaceCodes(&record, replacements))
            ++modified_count;

        marc_writer->write(record);
    }

    std::cerr << "Read " << total_count << " records.\n";
    std::cerr << "Modified " << modified_count << " record(s).\n";
}


void CollectReplacements(int argc, char **argv, std::vector<Replacement> * const replacements) {
    for (int arg_no(3); arg_no < argc; ++arg_no) {
        const std::string replacement_pattern(argv[arg_no]);
        if (replacement_pattern.length() != 6 or replacement_pattern[4] != '=')
            Error("bad replacement pattern: \"" + replacement_pattern + "\"!");
        replacements->emplace_back(replacement_pattern.substr(0, 3), replacement_pattern[3], replacement_pattern[5]);
    }
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 4)
        Usage();

    MarcReader::ReaderType reader_type(MarcReader::AUTO);
    if (StringUtil::StartsWith(argv[1], "--input-format=marc-")) {
        if (argc < 5)
            Usage();
        if (std::strcmp(argv[1], "--input-format=marc-21") == 0)
            reader_type = MarcReader::BINARY;
        else if (std::strcmp(argv[1], "--input-format=marc-xml") == 0)
            reader_type = MarcReader::XML;
        else
            Error("invalid reader type \"" + std::string(argv[1] + std::strlen("--input-format=")) + "\"!");
        ++argv;
    }

    try {
        std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1], reader_type));
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(argv[2]));

        std::vector<Replacement> replacements;
        CollectReplacements(argc, argv, &replacements);
        if (replacements.empty())
            Error("need at least one replacement pattern!");

        ReplaceCodes(marc_reader.get(), marc_writer.get(), replacements);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
