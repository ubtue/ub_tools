/** \file    convert_dakar.cc
 *  \brief   Augment the DAKAR database with authority data references for authors, keywords and
             CIC (Codex Iuris Canonici) references
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2018,2019 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "IniFile.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "util.h"


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "dakar.conf");
const std::string NOT_AVAILABLE("N/A");
typedef std::tuple<std::string, unsigned, unsigned> gnd_role_and_year;


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "--generate-list authority_data\n"
        "--augment-db authority_data [find_of_discovery_map_file bishop_rewrite_map official_rewrite_map hinweissätze_rewrite_map "
        "keyword_correction_map author_correction_map]\n"
        "--augment-db-keep [--keep-a_gnd] authority_data [find_of_discovery_map_file bishop_rewrite_map official_rewrite_map "
        "hinweissätze_rewrite_map keyword_correction_map author_correction_map]\n"
        "    no operation mode means --augment-db");
}


DbResultSet ExecSqlAndReturnResultsOrDie(const std::string &select_statement, DbConnection * const db_connection) {
    db_connection->queryOrDie(select_statement);
    return db_connection->getLastResultSet();
}


void GetAuthorsFromDB(DbConnection &db_connection, std::set<std::string> * const authors) {
    std::string distinct_author_query("SELECT DISTINCT autor FROM ikr");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(distinct_author_query, &db_connection));
    while (const DbRow db_row = result_set.getNextRow()) {
        const std::string author_row(db_row["autor"]);
        std::vector<std::string> authors_in_row;
        StringUtil::Split(author_row, ';', &authors_in_row, /* suppress_empty_components = */ true);
        for (auto &author : authors_in_row) {
            // Remove superfluous additions
            static std::regex to_strip("\\(Hrsg[\\.]\\)");
            author = std::regex_replace(author, to_strip, std::string(""));
            authors->emplace(StringUtil::TrimWhite(author));
        }
    }
}


void GetKeywordsFromDB(DbConnection &db_connection, std::set<std::string> * const keywords) {
    std::string distinct_keyword_query("SELECT DISTINCT stichwort FROM ikr");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(distinct_keyword_query, &db_connection));
    while (const DbRow db_row = result_set.getNextRow()) {
        const std::string keyword_row(db_row["stichwort"]);
        std::vector<std::string> keywords_in_row;
        StringUtil::Split(keyword_row, ';', &keywords_in_row, /* suppress_empty_components = */ true);
        for (const auto &keyword : keywords_in_row) {
            // Special Handling: He have entries that are erroneously separated by commas instead of semicolon
            std::vector<std::string> comma_separated_keywords;
            StringUtil::Split(keyword, ',', &comma_separated_keywords, /* suppress_empty_components = */ true);
            for (const auto comma_separated_keyword : comma_separated_keywords)
                keywords->emplace(StringUtil::TrimWhite(comma_separated_keyword));
        }
    }
}


void GetCICFromDB(DbConnection &db_connection, std::set<std::string> * const cic_numbers) {
    std::string distinct_cic_query("SELECT DISTINCT cicbezug FROM ikr");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(distinct_cic_query, &db_connection));
    while (const DbRow db_row = result_set.getNextRow()) {
        const std::string cic_row(db_row["cicbezug"]);
        std::vector<std::string> cics_in_row;
        StringUtil::Split(cic_row, ';', &cics_in_row, /* suppress_empty_components = */ true);
        for (const auto &cic : cics_in_row) {
            cic_numbers->emplace(StringUtil::TrimWhite(cic));
        }
    }
}


void AssemblePrimaryAndSynonymKeywordEntry(const MARC::Record &record, const std::string gnd_number,
                                           std::unordered_multimap<std::string, std::string> * const keyword_to_gnd_map,
                                           const std::string primary_tag, const std::string subfield_spec, const std::string synonym_tag) {
    const std::string primary(StringUtil::Join(record.getSubfieldAndNumericSubfieldValues(primary_tag, subfield_spec), " "));
    if (not primary.empty()) {
        keyword_to_gnd_map->emplace(primary, gnd_number);
        // Also get "Verweisungsformen"
        for (const auto &field : record.getTagRange(synonym_tag)) {
            const MARC::Subfields subfields(field.getContents());
            const std::string synonym(StringUtil::Join(subfields.extractSubfieldsAndNumericSubfields(subfield_spec), " "));
            if (not synonym.empty()) {
                keyword_to_gnd_map->emplace(synonym, gnd_number);
            }
        }
    }
}


void ExtractAuthorityData(const std::string &authority_file, std::unordered_multimap<std::string, std::string> * const author_to_gnd_map,
                          std::unordered_multimap<std::string, std::string> * const keyword_to_gnd_map,
                          std::unordered_map<std::string, std::string> * const cic_to_gnd_map) {
    auto marc_reader(MARC::Reader::Factory(authority_file));
    auto authority_reader(marc_reader.get());
    while (const auto &record = authority_reader->read()) {
        std::string gnd_number;

        if (not MARC::GetGNDCode(record, &gnd_number))
            continue;

        // Authors
        const std::string author(StringUtil::Join(record.getSubfieldAndNumericSubfieldValues("100", "abcpnt9v"), " "));
        if (not author.empty()) {
            author_to_gnd_map->emplace(author, gnd_number);
            // Also add "Verweisungsform"
            for (const auto &field : record.getTagRange("400")) {
                const MARC::Subfields subfields(field.getContents());
                const std::string synonym(StringUtil::Join(subfields.extractSubfieldsAndNumericSubfields("abcpnt9v"), " "));
                if (not synonym.empty()) {
                    author_to_gnd_map->emplace(synonym, gnd_number);
                }
            }
            continue; // next entry
        }

        // CIC
        // Possible contents: number; number-number; number,number; number,number,number
        const std::string cic_110_field(StringUtil::Join(record.getSubfieldValues("110", "atf"), ","));
        if (cic_110_field == "Katholische Kirche,Codex iuris canonici,1983") {
            const std::string cic_code(StringUtil::Join(record.getSubfieldValues("110", 'p'), " "));
            if (not cic_code.empty()) {
                // Dakar uses '.' instead of ',' as a separator
                cic_to_gnd_map->emplace(StringUtil::Map(cic_code, ',', '.'), gnd_number);
                // We will not find reasonable keywords in this iteration
                continue;
            }
        }

        // Keywords
        AssemblePrimaryAndSynonymKeywordEntry(record, gnd_number, keyword_to_gnd_map, "110", "abcdgnptxz9v9g", "410");
        AssemblePrimaryAndSynonymKeywordEntry(record, gnd_number, keyword_to_gnd_map, "111", "abcdgnptxz9v9g", "411");
        AssemblePrimaryAndSynonymKeywordEntry(record, gnd_number, keyword_to_gnd_map, "130", "abcdgnptxz9v9g", "430");
        AssemblePrimaryAndSynonymKeywordEntry(record, gnd_number, keyword_to_gnd_map, "150", "abcdgnptxz9v9g", "450");
        AssemblePrimaryAndSynonymKeywordEntry(record, gnd_number, keyword_to_gnd_map, "151", "abcdgnptxz9v9g", "451");
    }
}


auto GenerateGNDLink = [](const std::string gnd) { return "http://d-nb.info/gnd/" + gnd; };

void MakeGNDLink(std::vector<std::string> * const gnds) {
    std::vector<std::string> formatted_values;
    std::transform(gnds->begin(), gnds->end(), std::back_inserter(formatted_values), GenerateGNDLink);
    gnds->swap(formatted_values);
}


// Wrapper for a single string to guarantee that the generation function is not implemented at two different places in the code
void MakeGNDLink(std::string * const gnd) {
    std::vector<std::string> temp({ *gnd });
    MakeGNDLink(&temp);
    *gnd = temp[0];
}


void GetAuthorGNDResultMap(DbConnection &db_connection, const std::unordered_multimap<std::string, std::string> &all_authors_to_gnd_map,
                           std::unordered_map<std::string, std::string> * const author_to_gnds_result_map, const bool skip_empty = true,
                           const bool generate_gnd_link = false) {
    std::set<std::string> authors;
    GetAuthorsFromDB(db_connection, &authors);
    for (auto &author : authors) {
        auto author_and_gnds(all_authors_to_gnd_map.equal_range(author));
        std::vector<std::string> gnds;
        for (auto gnd(author_and_gnds.first); gnd != author_and_gnds.second; ++gnd)
            gnds.emplace_back(StringUtil::TrimWhite(gnd->second));
        if (not gnds.empty() or not skip_empty) {
            if (generate_gnd_link)
                MakeGNDLink(&gnds);
            author_to_gnds_result_map->emplace(author, StringUtil::Join(gnds, ","));
        }
    }
}


void GetKeywordGNDResultMap(DbConnection &db_connection, const std::unordered_multimap<std::string, std::string> &all_keywords_to_gnd_map,
                            std::unordered_map<std::string, std::string> *keyword_to_gnds_result_map, const bool skip_empty = true,
                            const bool generate_gnd_link = false) {
    std::set<std::string> keywords;
    GetKeywordsFromDB(db_connection, &keywords);
    for (auto &keyword : keywords) {
        const auto keyword_and_gnds(all_keywords_to_gnd_map.equal_range(keyword));
        std::vector<std::string> gnds;
        for (auto gnd(keyword_and_gnds.first); gnd != keyword_and_gnds.second; ++gnd)
            gnds.emplace_back(StringUtil::TrimWhite(gnd->second));
        if (not gnds.empty() or not skip_empty) {
            if (generate_gnd_link)
                MakeGNDLink(&gnds);
            keyword_to_gnds_result_map->emplace(keyword, StringUtil::Join(gnds, ","));
        }
    }
}


void GetCICGNDResultMap(DbConnection &db_connection, const std::unordered_map<std::string, std::string> &all_cics_to_gnd_map,
                        std::unordered_map<std::string, std::string> *cic_to_gnd_result_map, const bool skip_empty = true,
                        const bool generate_gnd_link = false) {
    std::set<std::string> cics;
    GetCICFromDB(db_connection, &cics);
    for (const auto &cic : cics) {
        const auto cic_and_gnd(all_cics_to_gnd_map.find(cic));
        if (cic_and_gnd != all_cics_to_gnd_map.cend()) {
            std::string cic_gnd(StringUtil::TrimWhite((*cic_and_gnd).second));
            if (generate_gnd_link)
                MakeGNDLink(&cic_gnd);
            cic_to_gnd_result_map->emplace(cic, cic_gnd);
        } else if (not skip_empty)
            cic_to_gnd_result_map->emplace(cic, "");
    }
}


std::pair<std::string, std::string> ExtractPPNAndDiscoverAbbrev(const std::vector<std::string> &line) {
    return std::make_pair(line[0], line[1]);
}


std::pair<std::string, std::string> ExtractHinttermAndCircumscription(const std::vector<std::string> &line) {
    return std::make_pair(line[0], line[1]);
}


std::pair<std::string, std::string> ExtractKeywordCorrection(const std::vector<std::string> &line) {
    if (line.size() >= 2 and not line[0].empty() and not line[1].empty())
        return std::make_pair(line[0], line[1]);
    return std::make_pair("", "");
}


std::pair<std::string, std::string> ExtractKeywordGNDCorrection(const std::vector<std::string> &line) {
    if (line.size() >= 3)
        return std::make_pair(line[0], line[2]);
    return std::make_pair("", "");
}


std::pair<std::string, std::string> ExtractAuthorGNDCorrection(const std::vector<std::string> &line) {
    // Only extract lines with existing GNDs
    if (line.size() >= 2 and not line[1].empty())
        return std::make_pair(line[0], line[1]);
    return std::make_pair("", "");
}


std::pair<std::string, gnd_role_and_year> ExtractBishopRoleYearAndGND(const std::vector<std::string> &line) {
    const std::string years_expression(line.size() >= 4 ? line[3] : "");
    std::vector<std::string> years;
    StringUtil::Split(years_expression, '-', &years);
    const unsigned year_lower(years.size() >= 1 and not years[0].empty() ? StringUtil::ToUnsigned(years[0]) : 0);
    const unsigned year_upper(years.size() == 2 and not years[1].empty() ? StringUtil::ToUnsigned(years[1])
                                                                         : StringUtil::ToUnsigned(TimeUtil::GetCurrentYear()));
    return std::make_pair(line[0], std::make_tuple(line[2], year_lower, year_upper));
}


std::pair<std::string, gnd_role_and_year> ExtractOfficialRoleYearAndGND(const std::vector<std::string> &line) {
    std::vector<std::string> years;
    const std::string years_expression(line.size() >= 3 ? line[2] : "");
    StringUtil::Split(years_expression, '-', &years);
    const unsigned year_lower(years.size() >= 1 and not years[0].empty() ? StringUtil::ToUnsigned(years[0]) : 0);
    const unsigned year_upper(years.size() == 2 and not years[1].empty() ? StringUtil::ToUnsigned(years[1])
                                                                         : StringUtil::ToUnsigned(TimeUtil::GetCurrentYear()));
    return std::make_pair(line[0], std::make_tuple(line[1], year_lower, year_upper));
}


void GenericGenerateTupleMultiMapFromCSV(
    const std::string &csv_filename, std::unordered_multimap<std::string, gnd_role_and_year> * const map,
    std::function<std::pair<std::string, gnd_role_and_year>(const std::vector<std::string>)> extractor) {
    std::vector<std::vector<std::string>> lines;
    TextUtil::ParseCSVFileOrDie(csv_filename, &lines);
    std::transform(lines.begin(), lines.end(), std::inserter(*map, map->begin()), extractor);
}


void GenericGenerateMapFromCSV(const std::string &csv_filename, std::unordered_map<std::string, std::string> * const map,
                               std::function<std::pair<std::string, std::string>(const std::vector<std::string>)> extractor,
                               const char separator = ',', const char quote = '"') {
    std::vector<std::vector<std::string>> lines;
    TextUtil::ParseCSVFileOrDie(csv_filename, &lines, separator, quote);
    std::transform(lines.begin(), lines.end(), std::inserter(*map, map->begin()), extractor);
}


void GenericGenerateMultiMapFromCSV(const std::string &csv_filename, std::unordered_multimap<std::string, std::string> * const map,
                                    std::function<std::pair<std::string, std::string>(const std::vector<std::string>)> extractor) {
    std::vector<std::vector<std::string>> lines;
    TextUtil::ParseCSVFileOrDie(csv_filename, &lines);
    std::transform(lines.begin(), lines.end(), std::inserter(*map, map->begin()), extractor);
}


void GetFindDiscoveryMap(const std::string &find_discovery_filename,
                         std::unordered_map<std::string, std::string> * const find_discovery_map) {
    GenericGenerateMapFromCSV(find_discovery_filename, find_discovery_map, ExtractPPNAndDiscoverAbbrev);
}


void GetBishopMap(const std::string &bishop_map_filename, std::unordered_multimap<std::string, gnd_role_and_year> * const bishop_map) {
    GenericGenerateTupleMultiMapFromCSV(bishop_map_filename, bishop_map, ExtractBishopRoleYearAndGND);
}


void GetOfficialsMap(const std::string &officials_map_filename,
                     std::unordered_multimap<std::string, gnd_role_and_year> * const officials_map) {
    GenericGenerateTupleMultiMapFromCSV(officials_map_filename, officials_map, ExtractOfficialRoleYearAndGND);
}


void GetHinttermsMap(const std::string &hintterm_map_filename, std::unordered_map<std::string, std::string> * const hintterms_map) {
    GenericGenerateMapFromCSV(hintterm_map_filename, hintterms_map, ExtractHinttermAndCircumscription, ':');
}


void AddKeywordTypoAndGNDCorrections(const std::string &keyword_correction_map_filename,
                                     std::unordered_map<std::string, std::string> * const keyword_correction_map,
                                     std::unordered_multimap<std::string, std::string> * const keyword_to_gnd_result_map) {
    GenericGenerateMapFromCSV(keyword_correction_map_filename, keyword_correction_map, ExtractKeywordCorrection);
    GenericGenerateMultiMapFromCSV(keyword_correction_map_filename, keyword_to_gnd_result_map, ExtractKeywordGNDCorrection);
}


void AddAuthorGNDCorrections(const std::string &author_correction_map_filenname,
                             std::unordered_multimap<std::string, std::string> * const author_correction_map) {
    GenericGenerateMultiMapFromCSV(author_correction_map_filenname, author_correction_map, ExtractAuthorGNDCorrection);
}


std::string ExtractAndFormatSource(const std::string &candidate, const std::string additional_information) {
    // Try to extract volume, year and pages
    std::string source(StringUtil::Trim(candidate));
    StringUtil::Map(&source, ",()=;", "     ");
    std::vector<std::string> components;
    StringUtil::Split(source, ' ', &components, true /* suppress empty components */);
    if (components.size() == 3)
        return StringUtil::Join(components, ", ");
    // Try to extract a year from the left side of the original match
    const std::regex plausible_year("\\b[12][901][0-9][0-9]\\b");
    std::smatch match;
    if (regex_search(additional_information, match, plausible_year))
        components.emplace_back(match.str());
    return StringUtil::Join(components, ", ");
}


// https://stackoverflow.com/questions/12200486/how-to-remove-duplicates-from-unsorted-stdvector-while-keeping-the-original-or (2019/12/10)
template <typename T>
std::vector<T> RemoveDuplicatesKeepOrder(std::vector<T> &vec) {
    std::unordered_set<T> seen;
    auto newEnd = std::remove_if(vec.begin(), vec.end(), [&seen](const T &value) {
        if (seen.find(value) != seen.end())
            return true;
        seen.insert(value);
        return false;
    });
    vec.erase(newEnd, vec.end());
    return vec;
}


bool ColumnExists(DbConnection * const db_connection, const std::string &column_name) {
    const std::string column_exists_query("SHOW COLUMNS FROM ikr LIKE \'" + column_name + "\'");
    return not ExecSqlAndReturnResultsOrDie(column_exists_query, db_connection).empty();
}


// Use this as a workaround for titles that lead to problems if handled case insensitively
const std::set<std::string> case_insensitive_blocked{ "Utrumque Ius" };


void AugmentDBEntries(DbConnection * const db_connection, const std::unordered_map<std::string, std::string> &author_to_gnds_result_map,
                      const std::unordered_map<std::string, std::string> &keyword_to_gnds_result_map,
                      const std::unordered_map<std::string, std::string> &cic_to_gnd_result_map,
                      const std::unordered_map<std::string, std::string> &find_discovery_map,
                      const std::unordered_multimap<std::string, gnd_role_and_year> &bishop_map,
                      const std::unordered_multimap<std::string, gnd_role_and_year> &officials_map,
                      const std::unordered_map<std::string, std::string> &hintterms_map,
                      const std::unordered_map<std::string, std::string> &keyword_correction_map, const bool keep_a_gnd)

{
    // Text the existence of originally newly added fields
    const bool f_ppn_exists(ColumnExists(db_connection, "f_ppn"));
    const bool f_quelle_exists(ColumnExists(db_connection, "f_quelle"));

    // Iterate over Database
    const std::string ikr_query("SELECT id,autor,stichwort,cicbezug,fundstelle,jahr,abstract"
                                + (keep_a_gnd ? std::string(",a_gnd") : std::string("")) + (f_ppn_exists ? std::string(",f_ppn") : "")
                                + (f_quelle_exists ? std::string(",f_quelle") : "") + +" FROM ikr");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(ikr_query, db_connection));
    while (const DbRow db_row = result_set.getNextRow()) {
        // Authors
        const std::string author_row(db_row["autor"]);
        std::vector<std::string> authors_in_row;
        std::vector<std::string> author_gnd_numbers;
        std::vector<std::string> authors_no_gnd;
        bool author_gnd_seen(false);
        StringUtil::SplitThenTrimWhite(author_row, ';', &authors_in_row, /* suppress_empty_components = */ true);
        for (const auto &one_author : authors_in_row) {
            const auto author_gnds(author_to_gnds_result_map.find(StringUtil::ReplaceString(" (Hrsg.)", "", one_author)));
            if (author_gnds != author_to_gnds_result_map.cend()) {
                author_gnd_numbers.emplace_back(author_gnds->second);
                author_gnd_seen = true;
            } else
                authors_no_gnd.emplace_back(StringUtil::Escape('\\', '"', one_author));
        }
        // If in keep_a_gnd mode, we keep the a_gnd
        std::string a_gnd_content;
        if (keep_a_gnd and not db_row["a_gnd"].empty())
            a_gnd_content = db_row["a_gnd"];
        else {
            // Only write back non-empty string if we have at least one reasonable entry
            a_gnd_content = author_gnd_seen ? StringUtil::Join(author_gnd_numbers, ";") : "";
        }
        std::string a_no_gnd_content(authors_no_gnd.size() ? StringUtil::Join(authors_no_gnd, ';') : "");

        // Apply manually fixed typos and circumscriptions
        std::string keyword_row(db_row["stichwort"]);
        std::vector<std::string> keywords_in_row;
        StringUtil::SplitThenTrimWhite(keyword_row, ";,", &keywords_in_row);
        for (auto keyword(keywords_in_row.begin()); keyword != keywords_in_row.end(); ++keyword) {
            const auto &corrected_term = keyword_correction_map.find(*keyword);
            if (corrected_term != keyword_correction_map.cend())
                *keyword = corrected_term->second;
        }
        keyword_row = StringUtil::Join(keywords_in_row, ';');

        // Replace Hinweissätze if present
        StringUtil::SplitThenTrimWhite(keyword_row, ";,", &keywords_in_row);
        for (auto keyword(keywords_in_row.begin()); keyword != keywords_in_row.end(); ++keyword) {
            const auto &hintterm = hintterms_map.find(*keyword);
            if (hintterm != hintterms_map.cend()) {
                *keyword = StringUtil::Map(hintterm->second, '/', ';');
            }
        }
        keyword_row = StringUtil::Join(keywords_in_row, ';');

        // Keywords
        StringUtil::SplitThenTrimWhite(keyword_row, ';', &keywords_in_row); // Get properly split keyword vector
        std::vector<std::string> keyword_gnd_numbers;
        std::vector<std::string> keywords_no_gnd;
        bool keyword_gnd_seen(false);
        StringUtil::Split(keyword_row, ';', &keywords_in_row, /* suppress_empty_components = */ true);
        keywords_in_row = RemoveDuplicatesKeepOrder(keywords_in_row);

        for (const auto one_keyword : keywords_in_row) {
            const auto keyword_gnds(keyword_to_gnds_result_map.find(one_keyword));
            if (keyword_gnds != keyword_to_gnds_result_map.cend()) {
                keyword_gnd_numbers.emplace_back(keyword_gnds->second);
                keyword_gnd_seen = true;
            } else
                keywords_no_gnd.emplace_back(one_keyword);
        }
        // Only write back non-empty string if we have at least one reasonable entry
        const std::string s_gnd_content(keyword_gnd_seen ? StringUtil::Join(keyword_gnd_numbers, ";") : "");
        const std::string s_no_gnd_content(keywords_no_gnd.size() ? StringUtil::Join(keywords_no_gnd, ";") : "");
        keyword_row = StringUtil::Join(keywords_in_row, ';');


        // CIC
        const std::string cic_row(db_row["cicbezug"]);
        std::vector<std::string> cics_in_row;
        std::vector<std::string> cic_gnd_numbers;
        bool cic_gnd_seen(false);
        StringUtil::Split(cic_row, ';', &cics_in_row, /* suppress_empty_components = */ true);
        for (const auto one_cic : cics_in_row) {
            const auto cic_gnd(cic_to_gnd_result_map.find(StringUtil::TrimWhite(one_cic)));
            if (cic_gnd != cic_to_gnd_result_map.cend()) {
                cic_gnd_numbers.emplace_back(cic_gnd->second);
                cic_gnd_seen = true;
            } else
                cic_gnd_numbers.emplace_back(NOT_AVAILABLE);
        }
        // Only write back non-empty string if we have at least one reasonable entry
        const std::string c_gnd_content(cic_gnd_seen ? StringUtil::Join(cic_gnd_numbers, ";") : "");

        // Fundstellen
        const std::string fundstelle_row(db_row["fundstelle"]);
        std::string f_ppn;
        std::string f_quelle;

        // We have manual changes after our run on the original file......
        if (result_set.hasColumn("f_ppn") and not db_row["f_ppn"].empty())
            f_ppn = db_row["f_ppn"];
        if (result_set.hasColumn("f_quelle") and not db_row["f_quelle"].empty())
            f_quelle = db_row["f_quelle"];
        else {
            for (const auto &entry : find_discovery_map) {
                size_t start, end;
                unsigned options(RegexMatcher::ENABLE_UTF8);
                options |=
                    case_insensitive_blocked.find(entry.second) == case_insensitive_blocked.end() ? RegexMatcher::CASE_INSENSITIVE : 0;
                if (RegexMatcher::Matched("(?<!\\pL)" + entry.second + "(?!\\pL)", fundstelle_row, options, nullptr, &start, &end)) {
                    f_ppn = entry.first;
                    f_quelle = ExtractAndFormatSource(fundstelle_row.substr(end), fundstelle_row.substr(0, start));
                    break;
                }
            }
        }

        // Map Bishops/Administrators role and year to personal GND number
        // In this context we hopefully don't have clashes if we split on comma
        StringUtil::SplitThenTrimWhite(author_row, ";,", &authors_in_row);
        const std::string year_row(db_row["jahr"]);
        std::vector<std::string> bishop_gnds;
        for (const auto &one_author : authors_in_row) {
            auto match_range(bishop_map.equal_range(one_author));
            for (auto gnd_and_years(match_range.first); gnd_and_years != match_range.second; ++gnd_and_years) {
                const unsigned year(StringUtil::ToUnsigned(year_row));
                if (std::get<1>(gnd_and_years->second) <= year and std::get<2>(gnd_and_years->second) >= year) {
                    const std::string gnd(std::get<0>(gnd_and_years->second));
                    bishop_gnds.emplace_back(gnd);
                    break;
                }
            }
        }
        if (not bishop_gnds.empty()) {
            const std::string gnds(StringUtil::Join(bishop_gnds, ','));
            a_gnd_content = not a_gnd_content.empty() ? a_gnd_content + ',' + gnds : gnds;
        }

        // Map Officials' role to personal GND number
        std::vector<std::string> officials_gnds;
        for (const auto &one_author : authors_in_row) {
            auto match_range(officials_map.equal_range(one_author));
            for (auto gnd_and_years(match_range.first); gnd_and_years != match_range.second; ++gnd_and_years) {
                unsigned year(std::atoi(year_row.c_str()));
                if (std::get<1>(gnd_and_years->second) <= year and std::get<2>(gnd_and_years->second) >= year) {
                    const std::string gnd(std::get<0>(gnd_and_years->second));
                    officials_gnds.emplace_back(gnd);
                    break;
                }
            }
        }
        if (not officials_gnds.empty()) {
            const std::string gnds(StringUtil::Join(officials_gnds, ','));
            a_gnd_content = not a_gnd_content.empty() ? a_gnd_content + ',' + gnds : gnds;
        }

        // Workaround for bishops/officials not yet known in original assignment of a_gnd
        if (not a_gnd_content.empty() and StringUtil::RemoveChars(" \\t", author_row) == StringUtil::RemoveChars(" \\t", a_no_gnd_content))
            a_no_gnd_content = "";


        // Extract Category from abstract
        const std::string abstract(db_row["abstract"]);
        static RegexMatcher * const category_matcher(RegexMatcher::RegexMatcherFactoryOrDie("([LRN])#"));
        std::string f_category_content;
        if (category_matcher->matched(abstract))
            f_category_content = (*category_matcher)[1];

        // Write back the new entries
        const std::string id(db_row["id"]);
        const std::string update_row_query("UPDATE ikr SET a_gnd=\"" + a_gnd_content + "\", a_no_gnd=\"" + a_no_gnd_content + "\", s_gnd=\""
                                           + s_gnd_content + "\", s_no_gnd=\"" + s_no_gnd_content + "\",c_gnd=\"" + c_gnd_content
                                           + "\",f_ppn=\"" + f_ppn + "\", f_quelle=\"" + f_quelle + "\", f_kategorie=\""
                                           + f_category_content + "\", stichwort=\"" + keyword_row + "\"" + " WHERE id=" + id);
        db_connection->queryOrDie(update_row_query);
    }
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();
    bool generate_list(false);
    bool skip_empty(true);                       // Do not insert entries without matches in the final lookup lists
    bool generate_gnd_link(false);               // Export GND numbers as links
    const bool use_find_discovery_map(true);     // Extract GND and vol, year, pages information
    const bool use_bishop_map(true);             // Map bishops as editors to their GND depending on their tenure
    const bool use_officials_map(true);          // Map officials to their GND
    const bool use_hintterms_map(true);          // Map Hinweissätze to circumscriptions
    const bool use_keyword_correction_map(true); // Correct and newly map keywords
    const bool use_author_correction_map(true);  // Correct and newly map authors
    bool keep_a_gnd(false);                      // Do not touch existing entries in the a_gnd field

    if (std::strcmp(argv[1], "--augment-db") == 0)
        --argc, ++argv;

    if (std::strcmp(argv[1], "--generate-list") == 0) {
        generate_list = true;
        skip_empty = false;
        generate_gnd_link = true;
        --argc, ++argv;
    }

    if (std::strcmp(argv[1], "--keep-a_gnd") == 0) {
        keep_a_gnd = true;
        --argc, ++argv;
    }

    if (argc < 2 and not generate_list)
        Usage();

    const std::string authority_file(argv[1]);
    --argc, ++argv;

    if (argc < 1 or argc != 7)
        Usage();

    const std::string find_discovery_map_filename(argv[1]);
    --argc, ++argv;

    const std::string bishop_map_filename(argv[1]);
    --argc, ++argv;

    const std::string officials_map_filename(argv[1]);
    --argc, ++argv;

    const std::string hintterms_map_filename(argv[1]);
    --argc, ++argv;

    const std::string keyword_corrections_map_filename(argv[1]);
    --argc, ++argv;

    const std::string author_corrections_map_filename(argv[1]);

    const IniFile ini_file(CONF_FILE_PATH);
    const std::string sql_database(ini_file.getString("Database", "sql_database"));
    const std::string sql_username(ini_file.getString("Database", "sql_username"));
    const std::string sql_password(ini_file.getString("Database", "sql_password"));
    DbConnection db_connection(sql_database, sql_username, sql_password);


    std::unordered_multimap<std::string, std::string> all_authors_to_gnd_map;
    std::unordered_multimap<std::string, std::string> all_keywords_to_gnds_map;
    std::unordered_map<std::string, std::string> keyword_correction_map;
    if (use_keyword_correction_map)
        AddKeywordTypoAndGNDCorrections(keyword_corrections_map_filename, &keyword_correction_map, &all_keywords_to_gnds_map);
    if (use_author_correction_map)
        AddAuthorGNDCorrections(author_corrections_map_filename, &all_authors_to_gnd_map);

    std::unordered_map<std::string, std::string> all_cics_to_gnd_map;
    ExtractAuthorityData(authority_file, &all_authors_to_gnd_map, &all_keywords_to_gnds_map, &all_cics_to_gnd_map);

    std::unordered_map<std::string, std::string> author_to_gnds_result_map;
    GetAuthorGNDResultMap(db_connection, all_authors_to_gnd_map, &author_to_gnds_result_map, skip_empty, generate_gnd_link);

    std::unordered_map<std::string, std::string> keyword_to_gnds_result_map;
    GetKeywordGNDResultMap(db_connection, all_keywords_to_gnds_map, &keyword_to_gnds_result_map, skip_empty, generate_gnd_link);

    std::unordered_map<std::string, std::string> cic_to_gnd_result_map;
    GetCICGNDResultMap(db_connection, all_cics_to_gnd_map, &cic_to_gnd_result_map, skip_empty, generate_gnd_link);
    if (generate_list) {
        std::ofstream author_out("/tmp/author_list.txt");
        for (const auto &author_and_gnds : author_to_gnds_result_map)
            author_out << author_and_gnds.first << "|" << author_and_gnds.second << '\n';
        std::ofstream keyword_out("/tmp/keyword_list.txt");
        for (const auto &keyword_and_gnds : keyword_to_gnds_result_map)
            keyword_out << keyword_and_gnds.first << "|" << keyword_and_gnds.second << '\n';
        std::ofstream cic_out("/tmp/cic_list.txt");
        for (const auto &cic_and_gnds : cic_to_gnd_result_map)
            cic_out << cic_and_gnds.first << "|" << cic_and_gnds.second << '\n';
    } else {
        std::unordered_map<std::string, std::string> find_discovery_map;
        if (use_find_discovery_map)
            GetFindDiscoveryMap(find_discovery_map_filename, &find_discovery_map);
        std::unordered_multimap<std::string, gnd_role_and_year> bishop_map;
        if (use_bishop_map)
            GetBishopMap(bishop_map_filename, &bishop_map);
        std::unordered_multimap<std::string, gnd_role_and_year> officials_map;
        if (use_officials_map)
            GetOfficialsMap(officials_map_filename, &officials_map);
        std::unordered_map<std::string, std::string> hintterms_map;
        if (use_hintterms_map)
            GetHinttermsMap(hintterms_map_filename, &hintterms_map);
        AugmentDBEntries(&db_connection, author_to_gnds_result_map, keyword_to_gnds_result_map, cic_to_gnd_result_map, find_discovery_map,
                         bishop_map, officials_map, hintterms_map, keyword_correction_map, keep_a_gnd);
    }
    return EXIT_SUCCESS;
}
