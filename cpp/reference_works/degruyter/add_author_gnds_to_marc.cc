/** \brief Augment de Gruyter MARC data with author-GNDs from ODGN-Lookup
 *  \author Johannes Riedl
 *
 *  \copyright 2022 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <iostream>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"

namespace {
[[noreturn]] void Usage() {
    ::Usage("marc_in marc_out authors_and_references.txt");
}


void AddAuthorsToRecord(MARC::Record * const record, const std::unordered_map<std::string, std::string> &names_and_gnds) {
    char _887_indicator2(0x30);
    for (const auto tag : { "100", "700" }) {
        for (auto &field : record->getTagRange(tag)) {
            const auto entry(names_and_gnds.find(field.getSubfields().getFirstSubfieldWithCode('a')));
            if (entry == names_and_gnds.end())
                continue;
            field.insertOrReplaceSubfield('0', "(DE-588)" + entry->second);
            char _887_indicator1 = ' ';
            if (++_887_indicator2 > 0x39) {
                _887_indicator1 = '1';
                _887_indicator2 = 0x30;
            }
            record->insertField(
                "887", MARC::Subfields({ MARC::Subfield('a', "Autor in der Vorlage [" + entry->first + "] maschinell zugeordnet") }),
                _887_indicator1, _887_indicator2);
        }
    }
}


void FixAuthorNameAbbrevs(MARC::Record * const record) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactoryOrDie("\\s+[A-Z]$"));
    for (const auto tag : { "100", "700" }) {
        for (auto &field : record->getTagRange(tag)) {
            const std::string author_orig(field.getSubfields().getFirstSubfieldWithCode('a'));
            if (matcher->matched(author_orig))
                field.insertOrReplaceSubfield('a', author_orig + '.');
        }
    }
}


void AugmentMarc(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                 const std::unordered_map<std::string, std::string> &names_and_gnds) {
    while (MARC::Record record = marc_reader->read()) {
        AddAuthorsToRecord(&record, names_and_gnds);
        FixAuthorNameAbbrevs(&record);
        marc_writer->write(record);
    }
}


void CreateNamesAndGNDsMap(const std::string &authors_and_gnds_path,
                           std::unordered_map<std::string, std::string> * const names_and_gnds_map) {
    const std::vector<std::string> names_and_gnds_lines(FileUtil::ReadLines::ReadOrDie(authors_and_gnds_path));
    for (const std::string &name_and_gnd_line : names_and_gnds_lines) {
        std::vector<std::string> name_and_gnd;
        if (StringUtil::Split(name_and_gnd_line, std::string(":\t"), &name_and_gnd)) {
            if (name_and_gnd.size() > 2)
                LOG_ERROR("Invalid number of elements in line \"" + name_and_gnd_line + "\"");
            if (name_and_gnd.size() == 1)
                continue;
            names_and_gnds_map->emplace(name_and_gnd[0], name_and_gnd[1]);
        }
    }
}

} // end unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const std::string marc_input_path(argv[1]);
    const std::string marc_output_path(argv[2]);
    const std::string authors_and_gnds_path(argv[3]);

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_path));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    std::unordered_map<std::string, std::string> names_and_gnds_map;
    CreateNamesAndGNDsMap(authors_and_gnds_path, &names_and_gnds_map);
    AugmentMarc(marc_reader.get(), marc_writer.get(), names_and_gnds_map);
    return EXIT_SUCCESS;
}
