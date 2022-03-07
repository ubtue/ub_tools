/** \file translation_exporter.cc
 *  \brief A tool creating authority data records from expert-translated keywords.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <iostream>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " authority_marc_file\n";
    std::exit(EXIT_FAILURE);
}


inline bool IsSynonym(const std::string &status) {
    return status == "replaced_synonym" or status == "new_synonym";
}


void GetMainAndAdditionalTranslations(const std::string &translation, std::string *main_translation, std::string *additional_translation) {
    main_translation->clear(), additional_translation->clear();

    const size_t first_lt_pos(translation.find('<'));
    if (first_lt_pos == std::string::npos)
        *main_translation = translation;
    else {
        const size_t first_gt_pos(translation.find('>'));
        if (unlikely(first_gt_pos == std::string::npos or first_gt_pos <= first_lt_pos + 1))
            logger->warning("malformed translation: \"" + translation + "\"!");
        else {
            *main_translation = StringUtil::RightTrim(translation.substr(0, first_lt_pos));
            *additional_translation = translation.substr(first_lt_pos + 1, first_gt_pos - first_lt_pos - 1);
        }
    }
}


void GenerateAuthortyRecords(DbConnection * const db_connection, MARC::Writer * const marc_writer) {
    db_connection->queryOrDie(
        "SELECT DISTINCT ppn FROM keyword_translations WHERE status='new' OR status='replaced'"
        " OR status='replaced_synonym' OR status='new_synonym'");
    DbResultSet ppn_result_set(db_connection->getLastResultSet());
    while (const DbRow ppn_row = ppn_result_set.getNextRow()) {
        const std::string ppn(ppn_row["ppn"]);
        const std::string status(ppn_row["status"]);
        db_connection->queryOrDie("SELECT language_code,translation FROM keyword_translations WHERE ppn='" + ppn
                                  + "' AND next_version_id IS NULL AND (status='new' OR status='replaced')");
        DbResultSet result_set(db_connection->getLastResultSet());

        MARC::Record new_record(MARC::Record::TypeOfRecord::AUTHORITY, MARC::Record::BibliographicLevel::UNDEFINED, ppn);

        while (const DbRow row = result_set.getNextRow()) {
            MARC::Subfields subfields;
            std::string main_translation, additional_translation;
            GetMainAndAdditionalTranslations(row["translation"], &main_translation, &additional_translation);
            subfields.addSubfield('a', main_translation);
            if (not additional_translation.empty())
                subfields.addSubfield('9', "g:" + additional_translation);
            subfields.addSubfield('9',
                                  "L:" + TranslationUtil::MapFake3LetterEnglishLanguagesCodesToGermanLanguageCodes(row["language_code"]));
            subfields.addSubfield('9', "Z:" + std::string(IsSynonym(status) ? "VW" : "AF"));
            subfields.addSubfield('2', "IxTheo");
            new_record.insertField("750", subfields);
        }

        marc_writer->write(new_record);
    }
}


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "translations.conf");


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[1]));

    const IniFile ini_file(CONF_FILE_PATH);
    const std::string sql_database(ini_file.getString("", "sql_database"));
    const std::string sql_username(ini_file.getString("", "sql_username"));
    const std::string sql_password(ini_file.getString("", "sql_password"));
    DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));

    GenerateAuthortyRecords(&db_connection, marc_writer.get());

    return EXIT_SUCCESS;
}
