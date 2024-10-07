/** \brief Tool for adding information about studies, that are not in K10Plus
 *         to 787.
 *  \author Johannes Riedl
 *
 *  \copyright 2024 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <string>
#include <vector>
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


using AdditionalInformation = std::pair<std::string, std::string>;
using MissingStudyInformation = std::map<std::string, AdditionalInformation>;

const std::string MISSING_TAG = "MIS";
static unsigned modified_count(0);

namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [-v|--verbose] marc_input study_information_txt marc_output\n";
    std::exit(EXIT_FAILURE);
}


void SetupMissingInformationLookupTable(File * const study_information_file, MissingStudyInformation * const study_information) {
    while (not study_information_file->eof()) {
        std::string line(study_information_file->getline());
        std::vector<std::string> id_title_authors;
        StringUtil::SplitThenTrimWhite(line, "\t", &id_title_authors);
        if (id_title_authors.size() < 2)
            LOG_ERROR("Invalid line: \"" + line + "\"");
        if (id_title_authors.size() == 2)
            study_information->insert(std::make_pair(id_title_authors[0], std::make_pair(id_title_authors[1], std::string(""))));
        else if (id_title_authors.size() == 3)
            study_information->insert(std::make_pair(id_title_authors[0], std::make_pair(id_title_authors[1], id_title_authors[2])));
        else
            LOG_ERROR("Invalid line [1]: \"" + line + "\"");
    }
}


void ProcessRecord(const bool verbose, MARC::Record * const record, const MissingStudyInformation &study_information) {
    if (not record->hasTag(MISSING_TAG))
        return;

    std::vector missing_study_ids(
        StringUtil::Split(record->getFirstSubfieldValue(MISSING_TAG, 'a'), ',', '\\' /* escape char */, true /*suppress empty*/));
    for (const auto &study_id : missing_study_ids) {
        if (verbose)
            LOG_INFO("Adding information for study " + study_id + " to record " + record->getControlNumber());
        const auto information(study_information.at(study_id));
        if (information.first.empty()) {
            record->insertFieldAtEnd("787", MARC::Subfields({ { 't', information.first } }));
            continue;
        }
        record->insertFieldAtEnd("787", MARC::Subfields({ { 'a', information.second }, { 't', information.first } }));
    }

    record->deleteFields("MIS");
}


void AddNonK10Plus787Information(bool verbose, MARC::Reader * const marc_reader, const MissingStudyInformation &study_information,
                                 MARC::Writer * const marc_writer) {
    unsigned record_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ProcessRecord(verbose, &record, study_information);
        marc_writer->write(record);
        ++record_count;
    }

    std::cout << "Modified " << modified_count << " of " << record_count << " record(s).\n";
}


} // unnamed namespace

int Main(int argc, char **argv) {
    if (argc < 3)
        Usage();

    const bool verbose(std::strcmp("-v", argv[1]) == 0 or std::strcmp("--verbose", argv[1]) == 0);
    if (verbose)
        --argc, ++argv;

    if (argc < 3)
        Usage();

    if (argc != 4)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto missing_information_file(FileUtil::OpenInputFileOrDie(argv[2]));
    auto marc_writer(MARC::Writer::Factory(argv[3]));

    MissingStudyInformation study_information;
    SetupMissingInformationLookupTable(missing_information_file.get(), &study_information);
    AddNonK10Plus787Information(verbose, marc_reader.get(), study_information, marc_writer.get());

    return EXIT_SUCCESS;
}
