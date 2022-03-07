/** \brief  A MARC-21 filter utility that adds ACO tags with entry $a set to 1 for article collections.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_set>
#include "MARC.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void CollectArticleCollectionPPNs(MARC::Reader * const reader, std::unordered_set<std::string> * const article_collection_ppns) {
    article_collection_ppns->clear();
    while (const MARC::Record record = reader->read()) {
        if (record.isArticle()) {
            const std::string parent_ppn(record.getParentControlNumber(/* additional_tags=*/{ "776" }));
            if (not parent_ppn.empty())
                article_collection_ppns->insert(parent_ppn);
        }
    }
}


// If any of the following matches, we have an essay collection:
struct EssayCollectionMatch {
    MARC::Tag tag_;
    char subfield_code_;
    std::string subfield_contents_;

public:
    EssayCollectionMatch(const MARC::Tag &tag, const char subfield_code, const std::string &subfield_contents)
        : tag_(tag), subfield_code_(subfield_code), subfield_contents_(subfield_contents) { }
};


const std::vector<EssayCollectionMatch> essay_collection_matches{
    { "935", 'c', "fe" },
    { "655", 'a', "Aufsatzsammlung" },
    { "689", 'a', "Aufsatzsammlung" },
    { "655", 'a', "Festschrift" },
    { "655", 'a', "Konferenzschrift" },
    { "689", 'a', "Konferenzschrift" },
    { "689", 'a', "Kongress" },
    { "935", 'c', "gkko" },
};


void MarkArticleCollections(MARC::Reader * const reader, MARC::Writer * const writer,
                            const std::unordered_set<std::string> &article_collection_ppns) {
    unsigned count(0), modified_count(0);
    while (MARC::Record record = reader->read()) {
        ++count;

        bool is_article_collection(false);
        if (article_collection_ppns.find(record.getControlNumber()) != article_collection_ppns.end())
            is_article_collection = true;
        if (not is_article_collection and not record.isArticle()) {
            for (const auto &field : record) {
                for (const auto &essay_collection_match : essay_collection_matches) {
                    if (field.getTag() == essay_collection_match.tag_
                        and field.hasSubfieldWithValue(essay_collection_match.subfield_code_, essay_collection_match.subfield_contents_))
                        goto found_article_collection;
                }
            }
        }


        if (is_article_collection) {
found_article_collection:
            record.insertField("ACO", { { 'a', "1" } });
            ++modified_count;
        }

        writer->write(record);
    }

    std::cout << ::progname << ": Read " << count << " records.\n";
    std::cout << ::progname << ": Identified " << modified_count << " record(s) as an article collection.\n";
}


} // unnamed namespace


int Main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[2]));

    std::unordered_set<std::string> article_collection_ppns;
    CollectArticleCollectionPPNs(marc_reader.get(), &article_collection_ppns);
    marc_reader->rewind();
    MarkArticleCollections(marc_reader.get(), marc_writer.get(), article_collection_ppns);

    return EXIT_SUCCESS;
}
