/** \brief A MARC-21 filter utility that selects records based on Library of Congress Subject Headings.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "FileUtil.h"
#include "MarcRecord.h"
#include "MarcReader.h"
#include "MarcWriter.h"
#include "MarcXmlWriter.h"
#include "MediaTypeUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " [[--input-format=(marc-xml|marc-21)]\n"
              << "       [--output-format=(marc-xml|marc-21)] marc_input marc_output subject_list\n\n"
              << "       where \"subject_list\" must contain LCSH's, one per line.\n";
    std::exit(EXIT_FAILURE);
}


void LoadSubjectHeadings(File * const input, std::unordered_set<std::string> * const loc_subject_headings) {
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        StringUtil::Trim(&line);
        if (not line.empty())
            loc_subject_headings->emplace(line);
    }
}


/** Returns true if we have at least one match in 650$a. */
bool Matched(const MarcRecord &record, const std::unordered_set<std::string> &loc_subject_headings) {
    std::vector<size_t> field_indices;
    if (record.getFieldIndices("650", &field_indices) == 0)
        return false;

    for (auto index : field_indices) {
        const Subfields subfields(record.getSubfields(index));
        std::string subfield_a(StringUtil::ToLower(subfields.getFirstSubfieldValue('a')));
        StringUtil::RightTrim(" .", &subfield_a);
        if (loc_subject_headings.find(subfield_a) != loc_subject_headings.cend())
            return true;
    }
    
    return false;
}


void Filter(MarcReader * const marc_reader, MarcWriter * const marc_writer,
            const std::unordered_set<std::string> &loc_subject_headings)
{
    unsigned total_count(0), matched_count(0);
    while (const MarcRecord record = marc_reader->read()) {
        ++total_count;
        if (Matched(record, loc_subject_headings)) {
            ++matched_count;
            marc_writer->write(record);
        }
    }

    std::cerr << "Processed a total of " << total_count << " record(s).\n";
    std::cerr << "Matched and therefore copied " << matched_count << " record(s).\n";
}


int main(int /*argc*/, char **argv) {
    ::progname = argv[0];
    ++argv;
    if (*argv == nullptr)
        Usage();

    MarcReader::ReaderType reader_type(MarcReader::AUTO);
    if (std::strcmp("--input-format=marc-xml", *argv) == 0) {
        reader_type = MarcReader::XML;
        ++argv;
    } else if (std::strcmp("--input-format=marc-21", *argv) == 0) {
        reader_type = MarcReader::BINARY;
        ++argv;
    }
    if (*argv == nullptr)
        Usage();

    MarcWriter::WriterType writer_type(MarcWriter::AUTO);
    if (std::strcmp("--output-format=marc-xml", *argv) == 0) {
        writer_type = MarcWriter::XML;
        ++argv;
    } else if (std::strcmp("--output-format=marc-21", *argv) == 0) {
        writer_type = MarcWriter::BINARY;
        ++argv;
    }
    if (*argv == nullptr)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(*argv++, reader_type));
    if (*argv == nullptr)
        Usage();

    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(*argv++, writer_type));
    if (*argv == nullptr)
        Usage();

    std::unique_ptr<File> subject_headings_file(FileUtil::OpenInputFileOrDie(*argv++));
    if (*argv != nullptr) // Should have been the last argument.
        Usage();

    try {
        std::unordered_set<std::string> loc_subject_headings;
        LoadSubjectHeadings(subject_headings_file.get(), &loc_subject_headings);

        Filter(marc_reader.get(), marc_writer.get(), loc_subject_headings);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
