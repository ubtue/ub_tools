/** \file    extract_vufind_translations_for_translation.cc
 *  \brief   A tool for extracting translations that need to be translated.  The keywords and any possibly
 *           pre-existing translations will be stored in an SQL database.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016,2017, Library of the University of Tübingen

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

#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "File.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " translation.ini...\n";
    std::exit(EXIT_FAILURE);
}

void InsertTranslations(
    DbConnection * const connection, const std::string &language_code,
    const std::unordered_map<std::string, std::pair<unsigned, std::string>> &keys_to_line_no_and_translation_map)
{
    for (const auto &keys_to_line_no_and_translation : keys_to_line_no_and_translation_map) {
        const std::string key = connection->escapeString(keys_to_line_no_and_translation.first);
        const std::string translation = connection->escapeString(keys_to_line_no_and_translation.second.second);

        // Skip inserting translations if we have a translator, i.e. the web translation tool was used
        // to insert the translations into the database
        const std::string GET_TRANSLATOR("SELECT translator FROM vufind_translations WHERE language_code=\""
           + TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes(language_code)
           + "\" AND token=\"" + key + "\"");
        connection->queryOrDie(GET_TRANSLATOR);
        DbResultSet result(connection->getLastResultSet());
        if (not result.empty()) {
            const DbRow row(result.getNextRow());
            if (not row.isNull("translator")) {
                const std::string translator(row["translator"]);
                if (not translator.empty())
                    continue;
            }
        }

        const std::string INSERT_OTHER(
           "REPLACE INTO vufind_translations SET language_code=\""
	   + TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes(language_code)
	   + "\", token=\"" + key + "\", translation=\"" + translation + "\"");
        connection->queryOrDie(INSERT_OTHER);
    }
}


const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc < 2)
        Usage();

    try {
        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("Database", "sql_database"));
        const std::string sql_username(ini_file.getString("Database", "sql_username"));
        const std::string sql_password(ini_file.getString("Database", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);

        for (int arg_no(1); arg_no < argc; ++arg_no) {
            // Get the 2-letter language code from the filename.  We expect filenames of the form "xx.ini" or
            // "some_path/xx.ini":
            const std::string ini_filename(argv[arg_no]);
            if (unlikely(not StringUtil::EndsWith(ini_filename, ".ini")))
                Error("expected filename \"" + ini_filename + "\" to end in \".ini\"!");
            std::string two_letter_code;
            if (ini_filename.length() == 6)
                two_letter_code = ini_filename.substr(0, 2);
            else {
                const std::string::size_type last_slash_pos(ini_filename.rfind('/'));
                if (unlikely(last_slash_pos == std::string::npos or (last_slash_pos + 6 + 1 != ini_filename.length())))
                    Error("INI filename does not match expected pattern: \"" + ini_filename + "\"!");
                two_letter_code = ini_filename.substr(last_slash_pos + 1, 2);
            }

            const std::string german_3letter_code(
                TranslationUtil::MapInternational2LetterCodeToGerman3LetterCode(two_letter_code));

            std::unordered_map<std::string, std::pair<unsigned, std::string>> keys_to_line_no_and_translation_map;
            TranslationUtil::ReadIniFile(ini_filename, &keys_to_line_no_and_translation_map);
            std::cout << "Read " << keys_to_line_no_and_translation_map.size()
                      << " mappings from English to another language from \"" << ini_filename << "\".\n";

            InsertTranslations(&db_connection, german_3letter_code, keys_to_line_no_and_translation_map);
        }
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
