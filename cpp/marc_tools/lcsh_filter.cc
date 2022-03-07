/** \brief A MARC-21 filter utility that selects records based on Library of Congress Subject Headings.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "usage: " << ::progname << " marc_input marc_output subject_list\n\n"
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
bool Matched(const MARC::Record &record, const std::unordered_set<std::string> &loc_subject_headings) {
    for (const auto &field : record.getTagRange("650")) {
        const auto subfields(field.getSubfields());
        std::string subfield_a(TextUtil::UTF8ToLower(subfields.getFirstSubfieldWithCode('a')));
        StringUtil::RightTrim(" .", &subfield_a);
        if (loc_subject_headings.find(subfield_a) != loc_subject_headings.cend())
            return true;
    }

    return false;
}


void Filter(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
            const std::unordered_set<std::string> &loc_subject_headings) {
    unsigned total_count(0), matched_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++total_count;
        if (Matched(record, loc_subject_headings)) {
            ++matched_count;
            marc_writer->write(record);
        }
    }

    std::cerr << "Processed a total of " << total_count << " record(s).\n";
    std::cerr << "Matched and therefore copied " << matched_count << " record(s).\n";
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));
    auto subject_headings_file(FileUtil::OpenInputFileOrDie(argv[3]));

    std::unordered_set<std::string> loc_subject_headings;
    LoadSubjectHeadings(subject_headings_file.get(), &loc_subject_headings);

    Filter(marc_reader.get(), marc_writer.get(), loc_subject_headings);

    return EXIT_SUCCESS;
}
