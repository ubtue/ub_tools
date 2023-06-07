/** \brief Add language information from output of detect_language_from_title.sh to given MARC file
 *  \author Johannes Riedl
 *
 *  \copyright 2023 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "util.h"


namespace {

using PPNToLanguageMap = std::unordered_map<std::string, std::string>;

[[noreturn]] void Usage() {
    ::Usage("language_association_file marc_input marc_output");
}

void CreateLanguageLookupMap(File * const detect_file, PPNToLanguageMap * const ppn_to_language_map) {
    while (not detect_file->eof()) {
        std::string line(detect_file->getline());
        const std::string ppn(StringUtil::ExtractHead(&line, ":"));
        const std::string language_code(StringUtil::ExtractHead(&line, ":"));
        if (not TranslationUtil::IsValidInternational2LetterCode(language_code)) {
            LOG_WARNING("Invalid 2 letter code \"" + language_code + "\"");
            continue;
        }
        ppn_to_language_map->emplace(
            std::make_pair(ppn, TranslationUtil::MapInternational2LetterCodeToGerman3Or4LetterCode(language_code)));
    }
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer, PPNToLanguageMap &ppn_to_language_map) {
    unsigned record_count(0);
    unsigned modified_count(0);
    while (MARC::Record record = marc_reader->read()) {
        const std::string ppn(record.getControlNumber());
        if (ppn_to_language_map.find(ppn) != ppn_to_language_map.end()) {
            const std::string language_code(ppn_to_language_map[ppn]);
            record.insertField("041", { { 'a', language_code } });
            ++modified_count;
        }
        marc_writer->write(record);
        ++record_count;
    }
    LOG_INFO("Modified " + std::to_string(modified_count) + " records of " + std::to_string(record_count));
}

} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    const std::unique_ptr<File> detect_file(FileUtil::OpenInputFileOrDie(argv[1]));
    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[2]));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[3]));

    PPNToLanguageMap ppn_to_language_map;
    CreateLanguageLookupMap(detect_file.get(), &ppn_to_language_map);
    ProcessRecords(marc_reader.get(), marc_writer.get(), ppn_to_language_map);
    return EXIT_SUCCESS;
}
