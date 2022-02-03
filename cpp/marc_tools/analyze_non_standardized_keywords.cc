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

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "TextUtil.h"
#include "util.h"


namespace {


std::string NormalizeKeyword(std::string keyword) {
    TextUtil::CollapseAndTrimWhitespace(&keyword);
    TextUtil::UTF8ToLower(&keyword);
    return keyword;
}


void CollectNormalizedKeywordsAndTranslations(MARC::Reader * const reader, std::unordered_set<std::string> * const normalized_keywords) {
    unsigned authority_record_count(0);
    while (const auto record = reader->read()) {
        if (record.getRecordType() != MARC::Record::RecordType::AUTHORITY)
            continue;

        ++authority_record_count;

        MARC::Record::KeywordAndSynonyms keyword_synonyms;
        if (record.getKeywordAndSynonyms(&keyword_synonyms)) {
            normalized_keywords->emplace(NormalizeKeyword(keyword_synonyms.getKeyword()));
            for (const auto &keyword_synonym : keyword_synonyms)
                normalized_keywords->emplace(NormalizeKeyword(keyword_synonym));
        }
    }

    LOG_INFO("Processed " + std::to_string(authority_record_count) + " authority records and found "
             + std::to_string(normalized_keywords->size()) + " normalized keywords and their translations.");
}


const std::vector<std::string> non_normalized_keyword_tags{ "650" };


void ProcessField(const MARC::Record::Field &field, const std::unordered_set<std::string> &normalized_keywords,
                  std::unordered_map<std::string, unsigned> * const unmatched_keywords_to_counts_map, unsigned * const matched_count,
                  unsigned * const not_matched_count) {
    const auto subfields(field.getSubfields());
    for (const auto &subfield_and_code : subfields) {
        if (subfield_and_code.code_ != 'a')
            continue;

        const auto non_normalized_keyword(NormalizeKeyword(subfield_and_code.value_));
        if (non_normalized_keyword.empty())
            continue;

        if (normalized_keywords.find(non_normalized_keyword) != normalized_keywords.cend())
            ++*matched_count;
        else {
            auto keyword_and_count(unmatched_keywords_to_counts_map->find(non_normalized_keyword));
            if (keyword_and_count != unmatched_keywords_to_counts_map->end())
                ++(keyword_and_count->second);
            else
                unmatched_keywords_to_counts_map->emplace(non_normalized_keyword, 1);
            ++*not_matched_count;
        }
    }
}


void ProcessTitleRecords(MARC::Reader * const marc_reader, const std::unordered_set<std::string> &normalized_keywords,
                         std::unordered_map<std::string, unsigned> * const unmatched_keywords_to_counts_map, unsigned * const matched_count,
                         unsigned * const not_matched_count) {
    unsigned record_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;
        for (const auto &tag : non_normalized_keyword_tags) {
            for (const auto &field : record.getTagRange(tag))
                ProcessField(field, normalized_keywords, unmatched_keywords_to_counts_map, matched_count, not_matched_count);
        }
    }

    LOG_INFO("Data set contains " + std::to_string(record_count) + " MARC record(s) of which ");
}


void ListUnmatchedKeywords(File * const output, const std::unordered_map<std::string, unsigned> &unmatched_keywords_to_counts_map) {
    std::vector<std::pair<std::string, unsigned>> unmatched_keywords_and_counts(unmatched_keywords_to_counts_map.cbegin(),
                                                                                unmatched_keywords_to_counts_map.cend());
    std::sort(unmatched_keywords_and_counts.begin(), unmatched_keywords_and_counts.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; }); // Sort in descending order of counts.

    for (const auto &[keyword, count] : unmatched_keywords_and_counts)
        (*output) << keyword << " -> " << count << '\n';
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        ::Usage("authority_records title_records keyword_stats_output");

    const auto authority_reader(MARC::Reader::Factory(argv[1]));
    std::unordered_set<std::string> normalized_keywords;
    CollectNormalizedKeywordsAndTranslations(authority_reader.get(), &normalized_keywords);

    const auto title_reader(MARC::Reader::Factory(argv[2]));
    std::unordered_map<std::string, unsigned> unmatched_keywords_to_counts_map;
    unsigned matched_count, not_matched_count;
    ProcessTitleRecords(title_reader.get(), normalized_keywords, &unmatched_keywords_to_counts_map, &matched_count, &not_matched_count);
    const double matched_percentage(100.0 * double(matched_count) / double(matched_count + not_matched_count));
    LOG_INFO("Found " + std::to_string(matched_percentage) + "% of the non-standardized keywords matched known, standardized keywords.");

    const auto output(FileUtil::OpenOutputFileOrDie(argv[3]));
    ListUnmatchedKeywords(output.get(), unmatched_keywords_to_counts_map);

    return EXIT_SUCCESS;
}
