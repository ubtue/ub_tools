/** \brief Utility for displaying various bits of info about a collection of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
    unsigned article_count_;
public:
    CollectionInfo(const std::string &shortened_title, const std::string &year)
        : shortened_title_(shortened_title), year_(year), article_count_(0) { }
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
    if (complete_title.length() > max_length) {
        if (unlikely(not TextUtil::UnicodeTruncate(&complete_title, max_length)))
            LOG_ERROR("bad Unicode in title of record with PPN " + record.getControlNumber() + "!");
        return complete_title + "...";
    }

    return complete_title;
}


bool IsPossibleYear(const std::string &year_candidate) {
    if (year_candidate.length() != 4)
        return false;

    for (auto ch : year_candidate) {
        if (not StringUtil::IsDigit(ch))
            return false;
    }

    return true;
}


std::string YYMMDateToString(const std::string &control_number, const std::string &yymm_date) {
    const unsigned CURRENT_YEAR(TimeUtil::GetCurrentTimeGMT().tm_year + 1900);
    const unsigned TWO_DIGIT_CURRENT_YEAR(CURRENT_YEAR - 2000);

    unsigned year_digits;
    if (StringUtil::ToUnsigned(yymm_date.substr(0, 2), &year_digits))
        return std::to_string(year_digits > TWO_DIGIT_CURRENT_YEAR ? 1900 + year_digits : 2000 + year_digits);
    LOG_WARNING("in yyMMDateToString: expected date in YYMM format, found \"" + yymm_date
                + "\" instead! (Control number was " + control_number + ")");
    return std::to_string(CURRENT_YEAR);
}


std::string GetDateForWebsite(const MARC::Record &record) {
    const auto _008_field(record.findTag("008"));
    const auto &control_number(record.getControlNumber());
    if (unlikely(_008_field == record.end()))
        LOG_ERROR("No 008 Field for website w/ control number " +  control_number + "!");
    return YYMMDateToString(control_number, _008_field->getContents());
}


std::string GetDateForReproduction(const MARC::Record &record) {
    const auto _534_field(record.findTag("534"));
    const auto &control_number(record.getControlNumber());
    if (unlikely(_534_field == record.end()))
        LOG_ERROR("No 534 Field for reproduction w/ control number " +  control_number + "!");

    const auto c_contents(_534_field->getFirstSubfieldWithCode('c'));
    if (c_contents.empty())
        return "";

    static const auto digit_matcher(RegexMatcher::RegexMatcherFactoryOrDie("(\\d+)"));
    return digit_matcher->matched(c_contents) ? (*digit_matcher)[1] : "";
}


std::string GetDateForArticleOrReview(const MARC::Record &record) {
    for (const auto &_936_field : record.getTagRange("936")) {
        const auto j_contents(_936_field.getFirstSubfieldWithCode('j'));
        if (not j_contents.empty()) {
            static const auto year_matcher(RegexMatcher::RegexMatcherFactoryOrDie("\\d{4}"));
            if (year_matcher->matched(j_contents))
                return (*year_matcher)[1];
        }
    }

    return "";
}


std::string GetDateFrom190j(const MARC::Record &record) {
    for (const auto &_190_field : record.getTagRange("190")) {
        const auto j_contents(_190_field.getFirstSubfieldWithCode('j'));
        if (likely(not j_contents.empty()))
            return j_contents;
        LOG_ERROR("No 190j subfield for PPN " + record.getControlNumber() + "!");
    }

    return "";
}


// Extract the sort date from the 008 field.
std::string GetSortDate(const MARC::Record &record) {
    const auto _008_field(record.findTag("008"));
    if (unlikely(_008_field == record.end()))
        LOG_ERROR("record w/ control number " + record.getControlNumber() + " is missing a 008 field!");

    const auto &_008_contents(_008_field->getContents());
    if (unlikely(_008_contents.length() < 12))
        return "";

    const auto year_candidate(_008_contents.substr(7, 4));
    if (unlikely(not IsPossibleYear(year_candidate)))
        LOG_ERROR("bad year in 008 field \"" + year_candidate + "\" for control number " + record.getControlNumber() + "!");
    return year_candidate;
}


std::string GetPublicationYear(const MARC::Record &record) {
    if (record.isWebsite())
        return GetDateForWebsite(record);

    if (record.isReproduction()) {
        const auto date(GetDateForReproduction(record));
        if (not date.empty())
            return date;
    }

    if ((record.isArticle() or MARC::IsAReviewArticle(record)) and not record.isMonograph()) {
        const auto date(GetDateForArticleOrReview(record));
        if (unlikely(date.empty()))
            LOG_ERROR("Could not find proper 936 field date content for record w/ control number " + record.getControlNumber() + "!");
        return date;
    }

    // Test whether we have a 190j field
    // This was generated in the pipeline for superior works that do not contain a reasonable 008(7,10) entry
    const std::string date(GetDateFrom190j(record));
    if (not date.empty())
        return date;

    return GetSortDate(record);
}


void ProcessRecords(const bool use_religious_studies_only, MARC::Reader * const marc_reader,
                    std::unordered_map<std::string, CollectionInfo> * const ppn_to_collection_info_map)
{
    unsigned record_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        if (use_religious_studies_only and record.findTag("REL") == record.end())
            continue;
        if (not IsCollection(record))
            continue;


        ppn_to_collection_info_map->emplace(record.getControlNumber(),
                                            CollectionInfo(GetShortenedTitle(record, 30), GetPublicationYear(record)));

        ++record_count;
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " MARC record(s).");
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

    const auto stats_output(FileUtil::OpenOutputFileOrDie(argv[3]));
    for (const auto &ppn_and_collection_info : ppn_to_collection_info_map)
        *stats_output << ppn_and_collection_info.first << ": " << ppn_and_collection_info.second.shortened_title_ << ", "
                      << ppn_and_collection_info.second.year_ << '\n';

    return EXIT_SUCCESS;
}
