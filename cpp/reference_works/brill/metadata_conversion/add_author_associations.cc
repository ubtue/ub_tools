/** \brief Add GND-PPN for authors and note in 887
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
#include "util.h"

namespace {
[[noreturn]] void Usage() {
    ::Usage("marc_in marc_out associations.txt");
}


void CreateAssociationMap(File * const association_file, std::map<std::string, std::string> * const association_map) {
    while (not association_file->eof()) {
        std::string line;
        association_file->getline(&line);
        std::vector<std::string> name_and_gnd;
        StringUtil::SplitThenTrimWhite(line, "|", &name_and_gnd);
        if (name_and_gnd.size() != 2)
            LOG_ERROR("Invalid number of elements in line \"" + line + "\"");
        association_map->emplace(std::make_pair(name_and_gnd[0], name_and_gnd[1]));
    }
}


void AugmentMarc(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                 const std::map<std::string, std::string> &associations) {
    while (MARC::Record record = marc_reader->read()) {
        for (const auto author_tag : { "100", "700" }) {
            for (auto &author_field : record.getTagRange(author_tag)) {
                auto author_subfields(author_field.getSubfields());
                const std::string author(author_subfields.getFirstSubfieldWithCode('a'));
                if (author.empty())
                    continue;
                author_subfields.appendSubfield('4', "aut");
                const auto association(associations.find(author));
                if (association == associations.end()) {
                    // Make sure $4aut is set
                    author_field.setSubfields(author_subfields);
                    continue;
                }
                std::string gnd(association->second);
                author_subfields.appendSubfield('0', "(DE-588)" + gnd);
                author_field.setSubfields(author_subfields);
                record.insertFieldAtEnd("887", { { 'a', "Autor [" + author + "] maschinell zugeordnet" }, { '2', "ixzom" } });
            }
        }
        marc_writer->write(record);
    }
}

} // end unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const std::string marc_input_path(argv[1]);
    const std::string marc_output_path(argv[2]);
    const std::string association_path(argv[3]);

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_path));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    const std::unique_ptr<File> association_file(FileUtil::OpenInputFileOrDie(association_path));

    std::map<std::string, std::string> associations;
    CreateAssociationMap(association_file.get(), &associations);
    AugmentMarc(marc_reader.get(), marc_writer.get(), associations);
    return EXIT_SUCCESS;
}
