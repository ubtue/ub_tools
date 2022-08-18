/** \brief Augment Bibelwissenschaften encyclopaedia MARC data with authors and references as extracted by the
 * extract_authors_and_references script \author Johannes Riedl
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
#include "MARC.h"
#include "TextUtil.h"
#include "util.h"

namespace {
[[noreturn]] void Usage() {
    ::Usage("type(=wibilex or wirelex) marc_in marc_out authors_and_references.csv");
}


void AddAuthorsToRecord(MARC::Record * const record, const std::unordered_map<std::string, std::vector<std::string>> &urls_and_authors) {
    const auto url(record->getFirstSubfieldValue("856", 'u'));
    const auto entry(urls_and_authors.find(url));
    if (entry == urls_and_authors.end()) {
        LOG_WARNING("Could not find entry for URL \"" + url + "\"");
        return;
    }

    if (entry->second.empty())
        LOG_ERROR("Empty Authors should not happen");

    bool first_author(true);
    for (const auto author : entry->second) {
        const std::string author_tag(first_author ? "100" : "700");
        first_author = false;
        MARC::Subfields author_subfields({ { 'a', author } });
        std::string author_gnd_candidate;
        ExecUtil::ExecSubcommandAndCaptureStdout("swb_author_lookup \"" + author + "\"", &author_gnd_candidate);
        if (not author_gnd_candidate.empty()) {
            author_subfields.appendSubfield('0', "(DE-588)" + StringUtil::Trim(author_gnd_candidate));
            record->insertFieldAtEnd("887", { { 'a', "Autor [" + author + "] maschinell zugeordnet" } });
        }
        author_subfields.appendSubfield('4', "aut");
        record->insertField(author_tag, author_subfields);
    }
}


void AugmentMarc(const std::string type, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                 const std::string &authors_and_references_path) {
    std::vector<std::vector<std::string>> authors_and_references;
    TextUtil::ParseCSVFileOrDie(authors_and_references_path, &authors_and_references);
    while (MARC::Record record = marc_reader->read()) {
        if (StringUtil::ASCIIToLower(record.getFirstSubfieldValue("TYP", 'a')) != type)
            continue;
        std::unordered_map<std::string, std::vector<std::string>> urls_and_authors;
        std::for_each(authors_and_references.begin(), authors_and_references.end(), [&](std::vector<std::string> line) {
            if (line[0] == "Author") {
                line[1].pop_back(); /*remove trailing '/'*/
                urls_and_authors.insert(std::make_pair(
                    line[1], std::vector<std::string>(line.begin() + 3 /*skip author/reference, link, title */, line.end())));
            }
        });
        AddAuthorsToRecord(&record, urls_and_authors);
        marc_writer->write(record);
    }
}

} // end unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 5)
        Usage();

    const std::string type(StringUtil::ASCIIToLower(argv[1]));
    if (type != "wibilex" and type != "wirelex")
        Usage();

    const std::string marc_input_path(argv[2]);
    const std::string marc_output_path(argv[3]);
    const std::string authors_and_references_path(argv[4]);

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_path));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    AugmentMarc(type, marc_reader.get(), marc_writer.get(), authors_and_references_path);
    return EXIT_SUCCESS;
}
