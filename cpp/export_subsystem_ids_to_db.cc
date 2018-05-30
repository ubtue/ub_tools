/** \file    export_subsystem_ids_to_db.cc
 *  \brief   Export PPN's of records tagged as specific subsystems to the VuFind MySQL database
 *           to allow filtering
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
#include "File.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "VuFind.h"
#include "util.h"


namespace {


enum Subsystem { RELBIB, BIBSTUDIES };
const std::vector<Subsystem> SUBSYSTEMS( { RELBIB, BIBSTUDIES } );


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--input-format=(marc-21|marc-xml)] marc_input\n";
    std::exit(EXIT_FAILURE);
}


void InsertToDatabase(DbConnection * const db_connection, std::string * const insert_statement) {
    // Remove trailing comma and space:
    insert_statement->resize(insert_statement->size() - 2);
    *insert_statement += ';';
    db_connection->queryOrDie(*insert_statement);
}


std::string GetSubsystemTag(const Subsystem subsystem) {
    const std::string RELBIB_TAG("REL");
    const std::string BIBSTUDIES_TAG("BIB");
    switch (subsystem) {
    case  RELBIB:
      return RELBIB_TAG;
    case BIBSTUDIES:
      return BIBSTUDIES_TAG;
    }
    LOG_ERROR("Invalid Subsystem " + std::to_string(subsystem));
}


std::string GetSubsystemName(const Subsystem subsystem) {
    switch (subsystem) {
    case  RELBIB:
      return "relbib";
    case BIBSTUDIES:
      return "bibstudies";
    }
    LOG_ERROR("Invalid Subsystem " + std::to_string(subsystem));
}


void InsertIntoSql(DbConnection * const db_connection, const Subsystem subsystem, const std::set<std::string> &subsystem_record_ids) {
    if (subsystem_record_ids.empty())
        return;

    const std::string subsystem_id_table(GetSubsystemName(subsystem) + "_ids");
    db_connection->queryOrDie("TRUNCATE " + subsystem_id_table);

    const std::string INSERT_STATEMENT_START("INSERT INTO " + subsystem_id_table + "(record_id) VALUES");
    std::string insert_statement(INSERT_STATEMENT_START);
    size_t row_counter(0);
    const size_t MAX_ROW_COUNT(10000);


    for (const auto &record_id : subsystem_record_ids) {
        insert_statement += "('" + record_id + "'), ";
        if (++row_counter > MAX_ROW_COUNT) {
            InsertToDatabase(db_connection, &insert_statement);
            insert_statement = INSERT_STATEMENT_START;
            row_counter = 0;
        }

    }
    InsertToDatabase(db_connection, &insert_statement);
}


void ExtractIDsForSubsystems(MARC::Reader * const marc_reader, std::vector<std::set<std::string>> * const subsystem_ids) {
    while (const MARC::Record &record = marc_reader->read()) {
        for (const Subsystem subsystem : SUBSYSTEMS) {
            if (not record.getTagRange(GetSubsystemTag(subsystem)).empty())
                ((*subsystem_ids)[subsystem]).emplace(record.getControlNumber());
        }
    }
}


void InitSubsystemsIDsVector(std::vector<std::set<std::string>> * const subsystems_ids) {
    for (unsigned i = 0; i < SUBSYSTEMS.size(); ++i) {
        std::set<std::string> init_set;
        subsystems_ids->push_back(init_set);
    }
}


} // unnamed namespace


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 2 and argc != 3)
        Usage();

    MARC::FileType reader_type(MARC::FileType::AUTO);
    if (argc == 3) {
        if (std::strcmp(argv[1], "--input-format=marc-21") == 0)
            reader_type = MARC::FileType::BINARY;
        else if (std::strcmp(argv[1], "--input-format=marc-xml") == 0)
            reader_type = MARC::FileType::XML;
        else
            Usage();
        ++argv, --argc;
    }

    const std::string marc_input_filename(argv[1]);
    unsigned imported_count(0);

    try {
        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);
        DbConnection db_connection(mysql_url);

        std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename, reader_type));
        std::vector<std::set<std::string>> subsystems_ids;
        InitSubsystemsIDsVector(&subsystems_ids);
        ExtractIDsForSubsystems(marc_reader.get(), &subsystems_ids);
        for (const auto subsystem : SUBSYSTEMS) {
            InsertIntoSql(&db_connection, subsystem, subsystems_ids[subsystem]);
            imported_count += subsystems_ids[subsystem].size();
        }


    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
    std::cerr << "Exported " << imported_count << " IDs to Database\n";
}


