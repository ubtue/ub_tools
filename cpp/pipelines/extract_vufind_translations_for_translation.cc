/** \file    extract_vufind_translations_for_translation.cc
 *  \brief   A tool for extracting translations that need to be translated.  The keywords and any possibly
 *           pre-existing translations will be stored in an SQL database.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016-2021, Library of the University of Tübingen

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
#include <map>
#include <string>
#include <unordered_map>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << progname << " translation.ini...\n";
    std::exit(EXIT_FAILURE);
}


void InsertTranslations(DbConnection * const connection, const std::string &language_code,
                        const std::unordered_map<std::string, std::pair<unsigned, std::string>> &keys_to_line_no_and_translation_map) {
    for (const auto &keys_to_line_no_and_translation : keys_to_line_no_and_translation_map) {
        const std::string key = connection->escapeString(keys_to_line_no_and_translation.first);
        const std::string translation = connection->escapeString(keys_to_line_no_and_translation.second.second);

        // Skip inserting translations if we have a translator, i.e. the web translation tool was used
        // to insert the translations into the database
        // successors get a prev_version_id value, successors should not be modified (if prev_version_id is not null)
        const std::string GET_TRANSLATOR("SELECT id, translator, next_version_id FROM vufind_translations WHERE language_code=\""
                                         + TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes(language_code)
                                         + "\" AND token=\"" + key + "\" AND prev_version_id IS NULL");
        connection->queryOrDie(GET_TRANSLATOR);
        DbResultSet result(connection->getLastResultSet());
        if (not result.empty()) {
            const DbRow row(result.getNextRow());
            // translator != null in a (prev_version_id==null)entry can happen due to previous logic
            if (not row.isNull("translator")) {
                const std::string translator(row["translator"]);
                if (not translator.empty())
                    continue;
            }
            // do not update original translations after they were modified for a successor
            // (even if translation is now 'b' and was before 'a' with a modification to 'a1')
            if (not row.isNull("next_version_id")) {
                const std::string next_version_id(row["next_version_id"]);
                if (not next_version_id.empty())
                    continue;
            }

            const std::string UPDATE_STMT("UPDATE vufind_translations SET translation=\"" + translation + "\" WHERE id=" + row["id"]);
            connection->queryOrDie(UPDATE_STMT);
        } else {
            const std::string INSERT_OTHER("INSERT INTO vufind_translations SET language_code=\""
                                           + TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes(language_code)
                                           + "\", token=\"" + key + "\", translation=\"" + translation + "\"");
            connection->queryOrDie(INSERT_OTHER);
        }
    }
}


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "translations.conf");


} // unnamed namespace


int Main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    const IniFile ini_file(CONF_FILE_PATH);
    const std::string sql_database(ini_file.getString("Database", "sql_database"));
    const std::string sql_username(ini_file.getString("Database", "sql_username"));
    const std::string sql_password(ini_file.getString("Database", "sql_password"));
    DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));

    for (int arg_no(1); arg_no < argc; ++arg_no) {
        // Get the 2-letter language code from the filename.  We expect filenames of the form "xx.ini" or
        // xx-yy.ini or "some_path/xx(-yy)?.ini
        const std::string ini_filename(argv[arg_no]);
        if (unlikely(FileUtil::GetExtension(ini_filename) != "ini"))
            LOG_ERROR("expected filename \"" + ini_filename + "\" to end in \".ini\"!");

        std::string two_letter_scheme(FileUtil::GetFilenameWithoutExtensionOrDie(FileUtil::GetBasename(ini_filename)));
        static ThreadSafeRegexMatcher expected_pattern_matcher("^[a-z]{2}(-[a-z]{2})?$");
        if (not expected_pattern_matcher.match(two_letter_scheme))
            LOG_ERROR("INI filename does not match expected pattern: \"" + ini_filename + "\"!");

        const std::string german_3letter_code(TranslationUtil::MapInternational2LetterCodeToGerman3Or4LetterCode(two_letter_scheme));

        std::unordered_map<std::string, std::pair<unsigned, std::string>> keys_to_line_no_and_translation_map;
        TranslationUtil::ReadIniFile(ini_filename, &keys_to_line_no_and_translation_map);
        std::cout << "Read " << keys_to_line_no_and_translation_map.size() << " mappings from English to another language from \""
                  << ini_filename << "\".\n";

        InsertTranslations(&db_connection, german_3letter_code, keys_to_line_no_and_translation_map);
    }

    return EXIT_SUCCESS;
}
