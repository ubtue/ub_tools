/** \file    convert_dakar.cc
 *  \brief   Augment the DAKAR database aith authority data refereces
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2016-2018, Library of the University of Tübingen

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
    std::string distinct_author_query("SELECT DISTINCT autor from ikr");
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


void ExtractAuthorsFromAuthorityData(const std::string &authority_file,
                                     std::unordered_multimap<std::string,std::string> * const author_to_gnd_map) {
    auto marc_reader(MARC::Reader::Factory(authority_file));
    auto authority_reader(marc_reader.get());
    while (const auto &record = authority_reader->read()) {
        std::vector<std::string> authors(record.getSubfieldValues("100", "a"));
        std::string gnd_number;
        if (MARC::GetGNDCode(record, &gnd_number)) {
               author_to_gnd_map->emplace(StringUtil::Join(authors, " "), gnd_number);
        }
    }
}

void GetAuthorGNDResultMap(DbConnection &db_connection, const std::string &authority_file,
                           std::unordered_multimap<std::string,std::string> * const author_to_gnds_result_map) {
     std::unordered_multimap<std::string,std::string> all_authors_to_gnd_map;
     ExtractAuthorsFromAuthorityData(authority_file, &all_authors_to_gnd_map);
     std::set<std::string> authors;
     GetAuthorsFromDB(db_connection, &authors);
     for (auto &author: authors) {
          auto author_and_gnds(all_authors_to_gnd_map.equal_range(author));
          for (auto gnd(author_and_gnds.first); gnd != author_and_gnds.second; ++gnd) {
              author_to_gnds_result_map->emplace(author, StringUtil::TrimWhite(gnd->second));
          }
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

     std::unordered_multimap<std::string,std::string> author_to_gnds_result_map;
     GetAuthorGNDResultMap(db_connection, authority_file, &author_to_gnds_result_map);

     for (const auto &author_and_gnds : author_to_gnds_result_map)
         std::cerr << author_and_gnds.first << "|||| " << author_and_gnds.second << '\n';
     std::cerr << "\n\n";
     //ExtractKeywordsFromAuthorityData(authority_file);
     //ExtractCICNotaitonFromAuthorityData(authority_file);

     return EXIT_SUCCESS;

}

