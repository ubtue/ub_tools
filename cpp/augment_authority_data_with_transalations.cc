/** \file    augment_authority_data_with_translations.cc
 *  \brief   Extract translations from database amd augment the normdata file
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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
#include <unordered_set>
#include <utility>
#include <vector>
#include <tuple>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "DirectoryEntry.h"
#include "IniFile.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MarcUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TimeUtil.h"
#include "TranslationUtil.h"
#include "util.h"

// Save ppn, language code, translation, and origin
typedef std::tuple<std::string, std::string, std::string, std::string> one_translation_t;
typedef std::vector<one_translation_t> translations_t;


void Usage() {
    std::cerr << "Usage: " << ::progname << " authority_data_input authority_data_output\n";
    std::exit(EXIT_FAILURE);
}


void ExecSqlOrDie(const std::string &select_statement, DbConnection * const connection) {
    if (unlikely(not connection->query(select_statement)))
        Error("SQL Statement failed: " + select_statement + " (" + connection->getLastErrorMessage() + ")");
}


void ExtractTranslations(DbConnection * const db_connection, std::vector<translations_t> * const all_translations) {
    ExecSqlOrDie("SELECT DISTINCT ppn FROM keyword_translations", db_connection);
    DbResultSet ppn_result_set(db_connection->getLastResultSet());
    while (const DbRow ppn_row = ppn_result_set.getNextRow()) {
        std::string ppn = ppn_row["ppn"];
        ExecSqlOrDie("SELECT ppn, language_code, translation, origin, status FROM keyword_translations WHERE ppn='" + ppn + "'",
                     db_connection);
        DbResultSet result_set(db_connection->getLastResultSet());
        std::vector<one_translation_t> translations;
        while (const DbRow row = result_set.getNextRow()) {
            // We are not interested in synonym fields
            if (row["status"] == "synonym")
                continue;
//            std::cout << ppn << "|" << row["language_code"] << ": " << row["translation"] << "|" << row["origin"] << '\n';
            one_translation_t translation(row["ppn"], row["language_code"], row["translation"], row["origin"]);
            translations.emplace_back(translation);
        }
        all_translations->emplace_back(translations);
        translations.clear();
    }
    return;
}


MarcWriter::WriterType DetermineOutputType(std::string filename) {
    if (StringUtil::EndsWith(filename, ".mrc"))
       return MarcWriter::BINARY;
    else if (StringUtil::EndsWith(filename, ".xml"))
       return MarcWriter::XML;
    else
       Error("Filename must end with \".mrc\" or \".xml\"!");
}


MarcReader::ReaderType DetermineInputType(std::string filename) {
    if (StringUtil::EndsWith(filename, ".mrc"))
       return MarcReader::BINARY;
    else if (StringUtil::EndsWith(filename, ".xml"))
       return MarcReader::XML;
    else
       Error("Filename must end with \".mrc\" or \".xml\"!");
}



void AugmentNormdata(MarcReader * const marc_reader, MarcWriter * marc_writer, const std::vector<translations_t> &all_translations) {

   MarcReader* tmp0 = marc_reader;
   MarcWriter* tmp1 = marc_writer;
   ++tmp0;
   ++tmp1;

   for (auto one_translation : all_translations) {
       std::cout << "NEXT ENTRY" << '\n';
       for (auto translation : one_translation) {
           std::cout << std::get<0>(translation) << ":" << std::get<1>(translation) << " " << std::get<2>(translation) << " " <<
                        std::get<3>(translation) <<  '\n';
       }
       std::cout << "------------------------------------------------------------" << '\n';
   }
}

const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);

    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Input file equals output file");

    try {
        std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename, DetermineInputType(marc_input_filename)));
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, DetermineOutputType(marc_output_filename)));

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("", "sql_database"));
        const std::string sql_username(ini_file.getString("", "sql_username"));
        const std::string sql_password(ini_file.getString("", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);

        std::vector<translations_t> all_translations;
        ExtractTranslations(&db_connection, &all_translations);

        AugmentNormdata(marc_reader.get(), marc_writer.get(), all_translations);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
