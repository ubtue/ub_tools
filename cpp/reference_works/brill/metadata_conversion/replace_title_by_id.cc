/** \brief Replace RGG4 titles by lookup table based on ID
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


#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "util.h"

namespace {

[[noreturn]] void Usage() {
    ::Usage("marc_in marc_out id_based_replacements.txt");
}


void CreateReplacementMap(File * const title_replacement_file, std::map<std::string, std::string> * const title_replacement_map) {
    while (not title_replacement_file->eof()) {
        std::string line;
        title_replacement_file->getline(&line);
        std::vector<std::string> id_and_title;
        StringUtil::SplitThenTrimWhite(line, "|", &id_and_title);
        if (id_and_title.size() != 2)
            LOG_ERROR("Invalid number of elements in line \"" + line + "\"");
        title_replacement_map->emplace(std::make_pair(id_and_title[0], id_and_title[1]));
    }
}


void AdjustTitles(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                  const std::map<std::string, std::string> &title_replacements) {
    unsigned modified(0);
    while (MARC::Record record = marc_reader->read()) {
        for (const auto &_024_field : record.getTagRange("024")) {
            auto _024_subfields(_024_field.getSubfields());
            if (_024_subfields.getFirstSubfieldWithCode('2') == "doi") {
                const std::string doi(_024_subfields.getFirstSubfieldWithCode('a'));
                static ThreadSafeRegexMatcher matcher(ThreadSafeRegexMatcher("((?:COM|DUM|SIM)_\\d+$)"));
                const auto match(matcher.match(doi));
                if (match) {
                    const auto id_and_title(title_replacements.find(match[1]));
                    if (id_and_title != title_replacements.end()) {
                        record.replaceField("245", MARC::Subfields({ { 'a', id_and_title->second } }), '1', '0');
                        ++modified;
                    }
                }
            } else
                continue;
        }
        marc_writer->write(record);
    }
    LOG_INFO("Modified " + std::to_string(modified) + " records");
}


} // End unnamed namespace

int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const std::string marc_input_path(argv[1]);
    const std::string marc_output_path(argv[2]);
    const std::string title_replacements_path(argv[3]);

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_path));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    const std::unique_ptr<File> title_replacement_file(FileUtil::OpenInputFileOrDie(title_replacements_path));

    std::map<std::string, std::string> title_replacements;
    CreateReplacementMap(title_replacement_file.get(), &title_replacements);
    AdjustTitles(marc_reader.get(), marc_writer.get(), title_replacements);
    return EXIT_SUCCESS;
}
