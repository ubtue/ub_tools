/** \file translation_exporter.cc
 *  \brief A tool creating authority data records from expert-translated keywords.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "FileUtil.h"
#include "IniFile.h"
#include "MarcRecord.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " authority_marc_file\n";
    std::exit(EXIT_FAILURE);
}


void ExecSqlOrDie(const std::string &select_statement, DbConnection * const connection) {
    if (unlikely(not connection->query(select_statement)))
        Error("SQL Statement failed: " + select_statement + " (" + connection->getLastErrorMessage() + ")");
}


void GenerateAuthortyRecords(DbConnection * const db_connection, MarcWriter * const marc_writer) {
    ExecSqlOrDie("SELECT DISTINCT ppn FROM keyword_translations WHERE status='new' OR status='replaced'",
                 db_connection);
    DbResultSet ppn_result_set(db_connection->getLastResultSet());
    while (const DbRow ppn_row = ppn_result_set.getNextRow()) {
        const std::string ppn(ppn_row["ppn"]);
        ExecSqlOrDie("SELECT language_code,translation FROM keyword_translations WHERE ppn='" + ppn
                     + "' AND (status='new' OR status='replaced')", db_connection);
        DbResultSet result_set(db_connection->getLastResultSet());

        MarcRecord new_record;
        new_record.getLeader().setRecordType(Leader::AUTHORITY);
        new_record.insertField("001", ppn);

        while (const DbRow row = result_set.getNextRow()) {
            Subfields subfields(' ', ' ');
            subfields.addSubfield('a', row["translation"]);
            subfields.addSubfield('9', "L:" + row["language_code"]);
            subfields.addSubfield('2', "ixtheo");
            new_record.insertField("750", subfields);
        }

        marc_writer->write(new_record);
    }
}


const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        if (argc != 2)
            Usage();

        const std::string authority_marc_file(argv[1]);
        const std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(authority_marc_file));

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("", "sql_database"));
        const std::string sql_username(ini_file.getString("", "sql_username"));
        const std::string sql_password(ini_file.getString("", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);

        GenerateAuthortyRecords(&db_connection, marc_writer.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
