/** \brief Utility for generating certain statistics for non-standardized keywords.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <unordered_set>
#include <cstdlib>
#include "MARC.h"
#include "util.h"


namespace {


void CollectNormalizedKeywordsAndTranslations(MARC::Reader * const reader,
                                              std::unordered_set<std::string> * const normalized_keywords)
{
    unsigned record_count(0);
    while (const auto record = reader->read()) {
        ++record_count;

        MARC::Record::KeywordAndSynonyms keyword_synonyms;
        if (record.getKeywordAndSynonyms(&keyword_synonyms)) {
            normalized_keywords->emplace(keyword_synonyms.getKeyword());
            normalized_keywords->insert(keyword_synonyms.begin(), keyword_synonyms.end());
        }
    }

    LOG_INFO("Processd " + std::to_string(record_count) + " authority records and found "
             + std::to_string(normalized_keywords->size()) + " normalized keywords and their translations.");
}


void ProcessTitleRecords(MARC::Reader * const marc_reader) {
    unsigned record_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;
    }

    LOG_INFO("Data set contains " + std::to_string(record_count) + " MARC record(s) of which ");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("authority_records title_records");

    const auto authority_records(MARC::Reader::Factory(argv[1]));
    std::unordered_set<std::string> normalized_keywords;
    CollectNormalizedKeywordsAndTranslations(authority_records.get(), &normalized_keywords);

    return EXIT_SUCCESS;
}
