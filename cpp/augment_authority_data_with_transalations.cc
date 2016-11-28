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

// Save language code, translation, origin, status
typedef std::tuple<std::string, std::string, std::string, std::string> one_translation_t;
// Save PPN as identifier
//typedef std::pair<std::string, std::vector<one_translation_t>> translations_t;

static unsigned record_count(0);


void Usage() {
    std::cerr << "Usage: " << ::progname << " authority_data_input authority_data_output\n";
    std::exit(EXIT_FAILURE);
}


inline bool IsSynonym(const std::string &status) {
    return status == "replaced_synonym" or status == "new_synonym";
}


void ExecSqlOrDie(const std::string &select_statement, DbConnection * const connection) {
    if (unlikely(not connection->query(select_statement)))
        Error("SQL Statement failed: " + select_statement + " (" + connection->getLastErrorMessage() + ")");
}


void ExtractTranslations(DbConnection * const db_connection, std::map<std::string, std::vector<one_translation_t>> * const all_translations) {
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
            if (IsSynonym(row["status"]))
                continue;
//            std::cout << ppn << "|" << row["language_code"] << ": " << row["translation"] << "|" << row["origin"] << '\n';
            one_translation_t translation(row["translation"], row["language_code"], row["origin"], row["status"]);
            translations.emplace_back(translation);
        }
        all_translations->insert(std::make_pair(ppn, translations));
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


void InsertTranslation(MarcRecord * const record, char indicator1, char indicator2, std::string term, std::string language_code, std::string status) {
    Subfields subfields(indicator1, indicator2);
    subfields.addSubfield('a', term);
    subfields.addSubfield('9', "L:" + language_code);
    subfields.addSubfield('9', "Z:" + std::string(IsSynonym(status) ? "VW" : "AF"));
    subfields.addSubfield('2', "IxTheo");
    record->insertField("750", subfields);
}


char DetermineNextFreeIndicator1(MarcRecord * const record, std::vector<size_t> field_indices) {
    char new_indicator1 = ' ';

    for (auto field_index: field_indices) {
        Subfields subfields(record->getSubfields(field_index));
        char indicator1 = subfields.getIndicator1();
        if (indicator1 == '9')
           Error("Indicator1 cannot be further incremented");
        new_indicator1 = indicator1 > new_indicator1 ? (indicator1 + 1) : new_indicator1;
    }

    return new_indicator1;
}


void ProcessRecord(MarcRecord * const record, const std::map<std::string, std::vector<one_translation_t>> &all_translations) {
     
     std::string ppn = record->getControlNumber(); 
     auto one_translation(all_translations.find(ppn));
     
     if (one_translation != all_translations.cend()) {
     
         // We only insert/replace IxTheo-Translations
         // For MACS Translations we insert a new field

         // What do we do if we have translations in several languages ??
         for (auto &one_lang_translation : one_translation->second) {
             std::string term(std::get<0>(one_lang_translation));
             std::string language_code(std::get<1>(one_lang_translation));
             std::string origin(std::get<2>(one_lang_translation));
             std::string status(std::get<3>(one_lang_translation));

             // See whether we already have a translation field
             std::vector<size_t> field_indices;
             if (record->getFieldIndices("750", &field_indices) > 0) {

                 // See whether a MACS translation already exists
                 for (auto field_index : field_indices) {
                     Subfields subfields_present = record->getSubfields(field_index);
                     if (subfields_present.hasSubfieldWithValue(2, "ram") or subfields_present.hasSubfieldWithValue(2, "lcsh")) {
                         // Insert field with fresh indicators
                         char indicator1(DetermineNextFreeIndicator1(record, field_indices));
                         InsertTranslation(record, indicator1, ' ', term, language_code, status);
                     }
                     else {
                         // For IxTheo-Terms, insert a potentially better translation
                         // FIXME: DELETE FIRST ??
                         char indicator1(subfields_present.getIndicator1());
                         char indicator2(subfields_present.getIndicator2());
                         InsertTranslation(record, indicator1, indicator2, term, language_code, status);
                     }
                 }
             }
             else {
                 char indicator1(DetermineNextFreeIndicator1(record, field_indices));
                 InsertTranslation(record, indicator1, ' ', term, language_code, status);
             }
        }
    }
}


void AugmentNormdata(MarcReader * const marc_reader, MarcWriter * marc_writer, const std::map<std::string, std::vector<one_translation_t>> &all_translations) {

   // Read in all PPNs from authority data

   // For a PPN in the bucket and see whether we can add a new translation 
   // First case: Add a new translation
   // Second case: Replace an existing translation
   while (MarcRecord record = marc_reader->read()) {
       ProcessRecord(&record, all_translations);
       marc_writer->write(record);
       ++record_count;
   }  


//   MarcReader* tmp0 = marc_reader;
//   MarcWriter* tmp1 = marc_writer;
//   ++tmp0;
//   ++tmp1;

/*   for (auto one_translation : all_translations) {
    //   std::cout << "NEXT ENTRY" << '\n';
       std::string ppn(one_translation.first);
       for (auto translation : one_translation.second) {
           
           std::cout  << ppn << ":" <<  std::get<0>(translation) << " " << std::get<1>(translation) << " " <<
                        std::get<2>(translation) <<  '\n';
       }
       std::cout << "------------------------------------------------------------" << '\n';
   }*/
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

        std::map<std::string, std::vector<one_translation_t>> all_translations;
        ExtractTranslations(&db_connection, &all_translations);

        AugmentNormdata(marc_reader.get(), marc_writer.get(), all_translations);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
