/** \brief Utility for displaying various bits of info about a collection of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <set>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace {


struct CollectionInfo {
    std::string shortened_title_, year_;
    bool is_toc_;
    unsigned article_count_;

public:
    CollectionInfo(const std::string &shortened_title, const std::string &year, const bool is_toc)
        : shortened_title_(shortened_title), year_(year), is_toc_(is_toc), article_count_(0) { }
    CollectionInfo() = default;
    CollectionInfo(const CollectionInfo &other) = default;
};


bool IsCollection(const MARC::Record &record) {
    for (const auto &_655_field : record.getTagRange("655")) {
        const auto a_contents(_655_field.getFirstSubfieldWithCode('a'));
        if (not a_contents.empty()) {
            if (a_contents == "Aufsatzsammlung" or a_contents == "Festschrift" or a_contents == "Konferenzschrift")
                return true;
        }
    }

    return false;
}


std::string GetShortenedTitle(const MARC::Record &record, const size_t max_length) {
    auto complete_title(record.getCompleteTitle());
    if (TextUtil::CodePointCount(complete_title) > max_length) {
        TextUtil::UTF8Truncate(&complete_title, max_length);
        return complete_title + "...";
    }

    return complete_title;
}


bool IsTOC(const MARC::Record &record) {
    for (const auto &_856_field : record.getTagRange("856")) {
        const MARC::Subfields subfields(_856_field.getSubfields());
        for (const auto &subfield : subfields) {
            if (subfield.code_ == '3' and (subfield.value_ == "Inhaltsverzeichnis" or subfield.value_ == "04"))
                return true;
        }
    }

    return false;
}


void ProcessRecords(const bool use_religious_studies_only, MARC::Reader * const marc_reader,
                    std::unordered_map<std::string, CollectionInfo> * const ppn_to_collection_info_map) {
    unsigned record_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        if (use_religious_studies_only
            and /*remove after migration*/ record.findTag("REL") == record.end() /*and not record.hasSubfieldWithValue("SUB", 'a', "REL")*/)
            continue;
        if (not IsCollection(record))
            continue;


        ppn_to_collection_info_map->emplace(
            record.getControlNumber(), CollectionInfo(GetShortenedTitle(record, 80), record.getMostRecentPublicationYear(), IsTOC(record)));

        ++record_count;
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " MARC record(s).");
}


void DetermineAttachedArticleCounts(const bool use_religious_studies_only, MARC::Reader * const marc_reader,
                                    std::unordered_map<std::string, CollectionInfo> * const ppn_to_collection_info_map) {
    while (const MARC::Record record = marc_reader->read()) {
        if (not record.isArticle())
            continue;
        if (use_religious_studies_only
            and /*remove after migration*/ record.findTag("REL") == record.end() /*and not record.hasSubfieldWithValue("SUB", 'a', "REL")*/)
            continue;

        const auto superior_control_number(record.getSuperiorControlNumber());
        const auto ppn_and_collection_info(ppn_to_collection_info_map->find(superior_control_number));
        if (ppn_and_collection_info != ppn_to_collection_info_map->end())
            ++ppn_and_collection_info->second.article_count_;
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        ::Usage("ixtheo|relbib marc_input stats_output");

    const std::string ssg(argv[1]);
    if (ssg != "ixtheo" and ssg != "relbib")
        LOG_ERROR("Sondersammelgebiet muss \"ixtheo\" oder \"relbib\" sein!");

    auto marc_reader(MARC::Reader::Factory(argv[2]));
    std::unordered_map<std::string, CollectionInfo> ppn_to_collection_info_map;
    ProcessRecords(ssg == "relbib", marc_reader.get(), &ppn_to_collection_info_map);

    marc_reader->rewind();

    DetermineAttachedArticleCounts(ssg == "relbib", marc_reader.get(), &ppn_to_collection_info_map);

    const auto stats_output(FileUtil::OpenOutputFileOrDie(argv[3]));
    for (const auto &ppn_and_collection_info : ppn_to_collection_info_map)
        *stats_output << ppn_and_collection_info.first << ": " << ppn_and_collection_info.second.shortened_title_ << ", "
                      << ppn_and_collection_info.second.year_ << ", " << (ppn_and_collection_info.second.is_toc_ ? "IHV" : "") << ", "
                      << ppn_and_collection_info.second.article_count_ << '\n';

    return EXIT_SUCCESS;
}
