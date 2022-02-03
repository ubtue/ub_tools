/** \brief  Generates a list of religious studies essay collections.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include "FileUtil.h"
#include "MARC.h"
#include "TextUtil.h"
#include "Unicode.h"


namespace {


void CollectArticleCollectionPPNs(MARC::Reader * const reader,
                                  std::unordered_map<std::string, unsigned> * const article_collection_ppns_and_counts) {
    article_collection_ppns_and_counts->clear();
    while (const MARC::Record record = reader->read()) {
        if (record.isArticle()) {
            const std::string parent_ppn(record.getParentControlNumber());
            if (parent_ppn.empty())
                continue;

            auto article_collection_ppn_and_count(article_collection_ppns_and_counts->find(parent_ppn));
            if (article_collection_ppn_and_count == article_collection_ppns_and_counts->end())
                article_collection_ppns_and_counts->insert({ parent_ppn, 1 });
            else
                ++(article_collection_ppn_and_count->second);
        }
    }
}


std::string ShortenTitle(const std::string &full_title, const size_t max_length) {
    const auto full_title_length(full_title.length());
    if (full_title_length <= max_length)
        return full_title;

    return full_title.substr(0, max_length - 3) + "...";
}


bool HasTOC(const MARC::Record &record) {
    for (const auto &_856_field : record.getTagRange("856")) {
        for (const auto &subfield : _856_field.getSubfields()) {
            if (subfield.code_ == '3' and ::strcasecmp(subfield.value_.c_str(), "Inhaltsverzeichnis") == 0)
                return true;
        }
    }

    return false;
}


bool IsMonographOfInterest(const MARC::Record &record) {
    if (not record.isMonograph())
        return false;

    for (const auto &_655_field : record.getTagRange("655")) {
        const auto a_subfield(_655_field.getFirstSubfieldWithCode('a'));
        if (::strcasecmp(a_subfield.c_str(), "Aufsatzsammlung") == 0 or ::strcasecmp(a_subfield.c_str(), "Festschrift") == 0
            or ::strcasecmp(a_subfield.c_str(), "Konferenzschrift") == 0)
            return true;
    }

    return false;
}


void MarkArticleCollections(MARC::Reader * const reader, File * const output,
                            const std::unordered_map<std::string, unsigned> &article_collection_ppns_and_counts) {
    *output << Unicode::UTF8_BOM;
    unsigned count(0);
    while (MARC::Record record = reader->read()) {
        if (IsMonographOfInterest(record)) {
            const auto collection_ppn_and_article_count(article_collection_ppns_and_counts.find(record.getControlNumber()));
            const unsigned article_count((collection_ppn_and_article_count != article_collection_ppns_and_counts.cend())
                                             ? collection_ppn_and_article_count->second
                                             : 0);

            const auto ssgns(record.getSSGNs());
            if (ssgns.find("0") != ssgns.cend()) {
                ++count;
                const auto publication_year(record.getMostRecentPublicationYear(/* fallback */ "????"));
                *output << TextUtil::CSVEscape(record.getControlNumber()) << '\t'
                        << TextUtil::CSVEscape(ShortenTitle(record.getMainTitle(), 60)) << '\t'
                        << TextUtil::CSVEscape((HasTOC(record) ? "Ja" : "Nein")) << '\t' << publication_year << '\t' << article_count
                        << '\n';
            }
        }
    }

    LOG_INFO("Found " + std::to_string(count) + " article collections.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        ::Usage("marc_input list");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    const auto output(FileUtil::OpenOutputFileOrDie(argv[2]));

    std::unordered_map<std::string, unsigned> article_collection_ppns_and_counts;
    CollectArticleCollectionPPNs(marc_reader.get(), &article_collection_ppns_and_counts);
    marc_reader->rewind();
    MarkArticleCollections(marc_reader.get(), output.get(), article_collection_ppns_and_counts);

    return EXIT_SUCCESS;
}
