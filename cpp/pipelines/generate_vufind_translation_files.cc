/** \file    generate_vufind_translation_files.cc
 *  \brief   A tool for creating the ".ini" files VuFind uses based on data in the SQL translations table.
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

#include <algorithm>
#include <iostream>
#include <map>
#include <tuple>
#include <utility>
#include <vector>
#include <cstring>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "File.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] output_directory_path\n";
    std::exit(EXIT_FAILURE);
}


// Needed since no consistent convention was used for brackets
std::string NormalizeBrackets(const std::string string_to_normalize) {
    std::set<std::string> skip_patterns({ "<br>", "<a href" });
    if (std::find_if(skip_patterns.begin(), skip_patterns.end(),
                     [&string_to_normalize](const auto &pattern) { return StringUtil::Contains(string_to_normalize, pattern); })
        != skip_patterns.end())
        return string_to_normalize;
    return StringUtil::Map(string_to_normalize, "<>", "()");
}


// Generates a XX.ini output file with entries like the original file.  The XX is a 2-letter language code.
void ProcessLanguage(const bool verbose, const std::string &output_file_path, const std::string &_3letter_code,
                     DbConnection * const db_connection) {
    if (verbose)
        std::cerr << "Processing language code: " << _3letter_code << '\n';

    std::unordered_map<std::string, std::pair<unsigned, std::string>> token_to_line_no_and_other_map;
    if (::access(output_file_path.c_str(), R_OK) != 0)
        LOG_WARNING("\"" + output_file_path + "\" is not readable, maybe it doesn't exist?");
    else {
        TranslationUtil::ReadIniFile(output_file_path, &token_to_line_no_and_other_map);

        if (unlikely(not FileUtil::RenameFile(output_file_path, output_file_path + ".bak", /* remove_target = */ true)))
            LOG_ERROR("failed to rename \"" + output_file_path + "\" to \"" + output_file_path + ".bak\"!");
    }

    File output(output_file_path, "w");
    if (unlikely(output.fail()))
        LOG_ERROR("failed to open \"" + output_file_path + "\" for writing!");

    db_connection->queryOrDie("SELECT token,translation FROM vufind_translations WHERE next_version_id IS NULL AND language_code='"
                              + _3letter_code + "'");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (unlikely(result_set.empty()))
        LOG_ERROR("found no translations for language code \"" + _3letter_code + "\"!");
    if (verbose)
        std::cerr << "\tFound " << result_set.size() << " (token,translation) pairs.\n";

    std::vector<std::tuple<unsigned, std::string, std::string>> line_nos_tokens_and_translations;
    while (const DbRow row = result_set.getNextRow()) {
        const auto &token_to_line_no_and_other(token_to_line_no_and_other_map.find(row[0]));
        if (token_to_line_no_and_other != token_to_line_no_and_other_map.cend())
            line_nos_tokens_and_translations.emplace_back(token_to_line_no_and_other->second.first, row[0], row[1]);
        else
            line_nos_tokens_and_translations.emplace_back(token_to_line_no_and_other_map.size() + 1, row[0], row[1]);
    }

    std::sort(line_nos_tokens_and_translations.begin(), line_nos_tokens_and_translations.end(),
              [](const std::tuple<unsigned, std::string, std::string> &left, const std::tuple<unsigned, std::string, std::string> &right) {
                  return std::get<0>(left) < std::get<0>(right);
              });

    for (const auto &line_no_token_and_translation : line_nos_tokens_and_translations) {
        const std::string token(std::get<1>(line_no_token_and_translation));
        const std::string translation(StringUtil::TrimWhite(std::get<2>(line_no_token_and_translation)));
        if (not translation.empty())
            output << token << " = \"" << StringUtil::TrimWhite(NormalizeBrackets(translation)) << "\"\n";
    }

    if (verbose)
        std::cerr << "Wrote " << line_nos_tokens_and_translations.size() << " language mappings to \"" << output_file_path << "\"\n";
}


void GetLanguageCodes(const bool verbose, DbConnection * const db_connection, std::map<std::string, std::string> *language_codes) {
    db_connection->queryOrDie("SELECT DISTINCT language_code FROM vufind_translations");
    DbResultSet language_codes_result_set(db_connection->getLastResultSet());
    if (unlikely(language_codes_result_set.empty()))
        LOG_ERROR("no language codes found, expected multiple!");

    while (const DbRow row = language_codes_result_set.getNextRow()) {
        const std::string german_language_code(TranslationUtil::MapFake3LetterEnglishLanguagesCodesToGermanLanguageCodes(row[0]));
        if (german_language_code == "???")
            continue;
        const std::string international_language_code(
            TranslationUtil::MapGerman3Or4LetterCodeToInternational2LetterCode(german_language_code));
        language_codes->emplace(international_language_code, row[0]);
    }
    if (verbose)
        std::cerr << "Found " << language_codes->size() << " distinct language code in the \"vufind_translations\" table.\n";
}


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "translations.conf");


} // unnamed namespace


int Main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();
    bool verbose(false);
    if (std::strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
        --argc, ++argv;
    }
    if (argc != 2)
        Usage();

    const std::string output_directory(argv[1]);
    if (unlikely(not FileUtil::IsDirectory(output_directory)))
        LOG_ERROR("\"" + output_directory + "\" is not a directory or can't be read!");

    const IniFile ini_file(CONF_FILE_PATH);
    const std::string sql_database(ini_file.getString("Database", "sql_database"));
    const std::string sql_username(ini_file.getString("Database", "sql_username"));
    const std::string sql_password(ini_file.getString("Database", "sql_password"));
    DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));

    std::map<std::string, std::string> _2letter_and_3letter_codes;
    GetLanguageCodes(verbose, &db_connection, &_2letter_and_3letter_codes);
    for (const auto &_2letter_intl_code_and_fake_3letter_english_code : _2letter_and_3letter_codes)
        ProcessLanguage(verbose, output_directory + "/" + _2letter_intl_code_and_fake_3letter_english_code.first + ".ini",
                        _2letter_intl_code_and_fake_3letter_english_code.second, &db_connection);

    return EXIT_SUCCESS;
}
