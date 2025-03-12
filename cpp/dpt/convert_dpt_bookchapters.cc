/** \brief Convert DPT Book chapters to MARC 21 Records
 *  \author Johannes Riedl
 *
 *  \copyright 2025 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <nlohmann/json.hpp>
#include "DbConnection.h"
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {

const char SEPARATOR_CHAR('|');

struct GNDAndName {
    std::string gnd_;
    std::string name_;
};

using DPTIDToGNDAndNameMap = std::unordered_map<std::string, GNDAndName>;


[[noreturn]] void Usage() {
    ::Usage("dpt_books.json author_dpt_id_gnd_mapping.txt marc_output");
}


void CreateIDToGNDAndNameMap(File * const mapping_file, DPTIDToGNDAndNameMap * const dpt_to_gnds_and_names) {
    while (not mapping_file->eof()) {
        std::string line;
        mapping_file->getline(&line);
        StringUtil::Trim(&line);
        std::vector<std::string> mapping;
        StringUtil::SplitThenTrim(line, SEPARATOR_CHAR, " \t", &mapping);
        if (unlikely(mapping.size() != 3)) {
            LOG_WARNING("Invalid line \"" + line + "\"");
            continue;
        }
        dpt_to_gnds_and_names->emplace(std::make_pair(mapping[0], GNDAndName({ mapping[1], mapping[2] })));
    }
}


MARC::Record *CreateNewRecord(const std::string &dpt_id) {
    std::ostringstream formatted_number;
    formatted_number << std::setfill('0') << std::setw(8) << std::atoi(dpt_id.c_str());
    const std::string prefix("DPT");
    const std::string ppn(prefix + formatted_number.str());

    return new MARC::Record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART, ppn);
}


void InsertTitle(MARC::Record * const marc_record, const std::string &title) {
    if (title.empty())
        return;
    marc_record->insertField("245", 'a', title);
}


void InsertAuthors(MARC::Record * const marc_record, const auto &authors, const DPTIDToGNDAndNameMap &dpt_to_gnds_and_names) {
    bool is_first_author(true);
    for (const auto &author : authors) {
        std::cout << "Author: " << author << '\n';
        const std::string author_id(author["ID"]);
        const auto gnd_and_name(dpt_to_gnds_and_names.find(author_id));
        if (gnd_and_name == dpt_to_gnds_and_names.end()) {
            LOG_WARNING("Unable to associate author with ID " + author_id);
            continue;
        }
        const std::string author_tag(is_first_author ? "100" : "700");
        const std::string &name(gnd_and_name->second.name_);
        const std::string &gnd(gnd_and_name->second.gnd_);

        if (gnd.empty()) {
            LOG_WARNING("No gnd given for Author ID " + author_id);
            continue;
        }

        if (name.empty()) {
            LOG_WARNING("No name given for Author ID " + author_id);
        }

        marc_record->insertField(author_tag, { { 'a', name }, { '0', "(DE-588)" + gnd } });

        is_first_author = is_first_author ? false : is_first_author;
    }
}


void ConvertArticles(MARC::Writer * const marc_writer, File * const dpt_books_file, const DPTIDToGNDAndNameMap &dpt_to_gnds_and_names) {
    std::ifstream dpt_books(dpt_books_file->getPath());
    nlohmann::json books_json(nlohmann::json::parse(dpt_books));

    for (const auto &book : books_json["Bücher"]) {
        for (const auto &chapter : book["Kapitel"]) {
            std::cout << chapter << "\n\n";
            const std::string dpt_id(chapter["ID"]);
            std::cout << "ID: " << chapter["ID"] << '\n';
            MARC::Record * const new_record(CreateNewRecord(dpt_id));
            const std::string title(chapter["Titel"]);
            std::cout << "Titel: " << chapter["Titel"] << '\n';
            InsertTitle(new_record, chapter["Titel"]);
            InsertAuthors(new_record, chapter["Autoren"], dpt_to_gnds_and_names);
            marc_writer->write(*new_record);
            delete new_record;
        }
    }
}

} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();
    const std::string dpt_books_file_path(argv[1]);
    const std::string dpt_id_gnd_mapping_file_path(argv[2]);
    const std::string marc_output_path(argv[3]);

    std::unique_ptr<File> dpt_books_file(FileUtil::OpenInputFileOrDie(dpt_books_file_path));
    std::unique_ptr<File> dpt_id_gnd_mapping_file(FileUtil::OpenInputFileOrDie(dpt_id_gnd_mapping_file_path));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    std::unordered_map<std::string, GNDAndName> dpt_ids_to_gnds_and_names;
    CreateIDToGNDAndNameMap(dpt_id_gnd_mapping_file.get(), &dpt_ids_to_gnds_and_names);
    ConvertArticles(marc_writer.get(), dpt_books_file.get(), dpt_ids_to_gnds_and_names);

    return EXIT_SUCCESS;
}
