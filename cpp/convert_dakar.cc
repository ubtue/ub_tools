/** \file    convert_dakar.cc
 *  \brief   Augment the DAKAR database with authority data references for authors, keywords and
             CIC (Codex Iuris Canonici) references
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2018, Library of the University of Tübingen

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
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "IniFile.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


const std::string CONF_FILE_PATH("/usr/local/var/lib/tuelib/dakar.conf");

namespace {

[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " authority_data output_file" << '\n';
    std::exit(EXIT_FAILURE);
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
        StringUtil::Split(author_row, ';', &authors_in_row);
        for (const auto &author : authors_in_row) {
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
        StringUtil::Split(keyword_row, ';', &keywords_in_row);
        for (const auto &keyword : keywords_in_row) {
            keywords->emplace(StringUtil::TrimWhite(keyword));
        }
    }
}


void GetCICFromDB(DbConnection &db_connection, std::set<std::string> * const cic_numbers) {
    std::string distinct_cic_query("SELECT DISTINCT cicbezug FROM ikr");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(distinct_cic_query, &db_connection));
    while (const DbRow db_row = result_set.getNextRow()) {
        const std::string cic_row(db_row["cicbezug"]);
        std::vector<std::string> cics_in_row;
        StringUtil::Split(cic_row, ';', &cics_in_row);
        for (const auto &cic : cics_in_row) {
            cic_numbers->emplace(StringUtil::TrimWhite(cic));
        }
    }
}


void ExtractAuthorityData(const std::string &authority_file,
                          std::unordered_multimap<std::string,std::string> * const author_to_gnd_map,
                          std::unordered_multimap<std::string,std::string> * const keyword_to_gnd_map,
                          std::unordered_map<std::string,std::string> * const cic_to_gnd_map)
{
    auto marc_reader(MARC::Reader::Factory(authority_file));
    auto authority_reader(marc_reader.get());
    while (const auto &record = authority_reader->read()) {
        std::string gnd_number;

        if (not MARC::GetGNDCode(record, &gnd_number))
            continue;

        // Authors
        const std::string author(StringUtil::Join(record.getSubfieldValues("100", "a"), " "));
        if (not author.empty()) {
            author_to_gnd_map->emplace(author, gnd_number);
        }

        // CIC
        // Possible contents: number; number-number; number,number; number,number,number
        const std::string cic_110_field(StringUtil::Join(record.getSubfieldValues("110", "atf"), ","));
        if (cic_110_field == "Katholische Kirche,Codex iuris canonici,1983") {
            const std::string cic_code(StringUtil::Join(record.getSubfieldValues("110", 'p'), " "));
std::cerr << "Found CIC PPN " << record.getControlNumber() << " for CIC: " << cic_code << '\n';
            if (not cic_code.empty()) {
                // Dakar uses '.' instead of ',' as a separator
                cic_to_gnd_map->emplace(StringUtil::Map(cic_code, ',', '.'), gnd_number);
                // We will not find reasonable keywords in this iteration
                continue;
            }
        }


        // Keywords
        const std::string keyword_110(StringUtil::Join(record.getSubfieldValues("110", "abcdnpt"), " "));
        const std::string keyword_111(StringUtil::Join(record.getSubfieldValues("111", "abcdnpt"), " "));
        const std::string keyword_130(StringUtil::Join(record.getSubfieldValues("130", "abcdnpt"), " "));
        const std::string keyword_150(StringUtil::Join(record.getSubfieldValues("150", "abcdnpt"), " "));
        if (not keyword_110.empty())
           keyword_to_gnd_map->emplace(keyword_110, gnd_number);
        if (not keyword_111.empty())
           keyword_to_gnd_map->emplace(keyword_111, gnd_number);
        if (not keyword_130.empty())
           keyword_to_gnd_map->emplace(keyword_130, gnd_number);
        if (not keyword_150.empty())
           keyword_to_gnd_map->emplace(keyword_150, gnd_number);
    }
}

void GetAuthorGNDResultMap(DbConnection &db_connection,
                           const std::unordered_multimap<std::string, std::string> &all_authors_to_gnd_map,
                           std::unordered_multimap<std::string,std::string> * const author_to_gnds_result_map)
{
     std::set<std::string> authors;
     GetAuthorsFromDB(db_connection, &authors);
     for (auto &author: authors) {
          auto author_and_gnds(all_authors_to_gnd_map.equal_range(author));
          for (auto gnd(author_and_gnds.first); gnd != author_and_gnds.second; ++gnd) {
              author_to_gnds_result_map->emplace(author, StringUtil::TrimWhite(gnd->second));
          }
     }
}


void GetKeywordGNDResultMap(DbConnection &db_connection,
                     const std::unordered_multimap<std::string, std::string> &all_keywords_to_gnd_map,
                     std::unordered_multimap<std::string,std::string> * keyword_to_gnds_result_map)
{
    std::set<std::string> keywords;
    GetKeywordsFromDB(db_connection, &keywords);
    for (auto &keyword : keywords) {
        const auto keyword_and_gnds(all_keywords_to_gnd_map.equal_range(keyword));
        for (auto gnd(keyword_and_gnds.first); gnd != keyword_and_gnds.second; ++gnd) {
            keyword_to_gnds_result_map->emplace(keyword, StringUtil::TrimWhite(gnd->second));
        }
    }
}


void GetCICGNDResultMap(DbConnection &db_connection,
                        const std::unordered_map<std::string, std::string> &all_cics_to_gnd_map,
                        std::unordered_map<std::string,std::string> * cic_to_gnd_result_map)
{
    std::set<std::string> cics;
    GetCICFromDB(db_connection, &cics);
    for (const auto &cic : cics) {
        const auto cic_and_gnd(all_cics_to_gnd_map.find(cic));
        if (cic_and_gnd != all_cics_to_gnd_map.cend())
           cic_to_gnd_result_map->emplace(cic, StringUtil::TrimWhite((*cic_and_gnd).second));
    }
}



} //unnamed namespace

int Main(int argc, char **argv) {
     if (argc != 3)
         Usage();

     const std::string authority_file(argv[1]);
     const std::string output_file(argv[2]);

     const IniFile ini_file(CONF_FILE_PATH);
     const std::string sql_database(ini_file.getString("Database", "sql_database"));
     const std::string sql_username(ini_file.getString("Database", "sql_username"));
     const std::string sql_password(ini_file.getString("Database", "sql_password"));
     DbConnection db_connection(sql_database, sql_username, sql_password);


     std::unordered_multimap<std::string, std::string> all_authors_to_gnd_map;
     std::unordered_multimap<std::string, std::string> all_keywords_to_gnds_map;
     std::unordered_map<std::string, std::string> all_cics_to_gnd_map;
     ExtractAuthorityData(authority_file, &all_authors_to_gnd_map, &all_keywords_to_gnds_map, &all_cics_to_gnd_map);

     std::unordered_multimap<std::string,std::string> author_to_gnds_result_map;
     GetAuthorGNDResultMap(db_connection, all_authors_to_gnd_map, &author_to_gnds_result_map);
     for (const auto &author_and_gnds : author_to_gnds_result_map)
         std::cerr << author_and_gnds.first << "||||" << author_and_gnds.second << '\n';
     std::cerr << "\n\n";

     std::unordered_multimap<std::string,std::string> keyword_to_gnds_result_map;
     GetKeywordGNDResultMap(db_connection, all_keywords_to_gnds_map, &keyword_to_gnds_result_map);
     for (const auto &keyword_and_gnds : keyword_to_gnds_result_map)
         std::cerr << keyword_and_gnds.first << "++++" << keyword_and_gnds.second << '\n';
     std::cerr << "\n\n";

     std::unordered_map<std::string,std::string> cic_to_gnd_result_map;
     GetCICGNDResultMap(db_connection, all_cics_to_gnd_map, &cic_to_gnd_result_map);
     for (const auto &cic_and_gnds : cic_to_gnd_result_map)
         std::cerr << cic_and_gnds.first << "****" << cic_and_gnds.second << '\n';

     return EXIT_SUCCESS;

}

