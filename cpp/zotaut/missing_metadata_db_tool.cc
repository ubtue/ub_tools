/** \brief Utility for performing certain edits on the metadata_presence_tracer MySQL table in ub_tools.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " (--clear-journals journal_list | (--ignore-fields|--sometimes-fields) journal_name field_list)\n"
              << "       \"journal_list\" is a comma-separated, case-sensitive list of journal names.  If you need to\n"
              << "       include a comma, or a backslash in a journal name you must backslash-escape it.\n"
              << "       \"--ignore-fields\" sets the \"field_presence\" column of one or more fields in the database\n"
              << "       to \"ignore\", \"--sometimes-fields\" does the same but sets the column(s) to \"sometimes\".\n"
              << "       \"field_list\" is a comma_separated list of field tags.\n\n";
    std::exit(EXIT_FAILURE);
}


// Splits a comma-separated list w/ possible backslash-escaped characters.
void UnescapeList(const std::string &escaped_list, std::vector<std::string> * const unescaped_list) {
    std::string current_entry;
    bool escaped(false);
    for (char ch : escaped_list) {
        if (escaped) {
            escaped = false;
            current_entry += ch;
        } else if (ch == '\\')
            escaped = true;
        else if (ch == ',') {
            if (unlikely(current_entry.empty()))
                LOG_ERROR("empty journal name specified on command-line!");
            unescaped_list->emplace_back(current_entry);
            current_entry.clear();
        } else
            current_entry += ch;
    }

    if (current_entry.empty())
        LOG_ERROR("empty journal name specified on command-line! (2)");
    unescaped_list->emplace_back(current_entry);
}


void ClearJournals(DbConnection * const db_connection, const std::string &escaped_journal_names) {
    std::vector<std::string> journal_names;
    UnescapeList(escaped_journal_names, &journal_names);

    for (const auto &journal_name : journal_names)
        db_connection->queryOrDie(
            "DELETE FROM metadata_presence_tracer WHERE journal_id='"
            "(SELECT id FROM zeder_journals WHERE journal_name="
            + db_connection->escapeAndQuoteString(journal_name) + ")");
}


void SetFieldsToIgnore(DbConnection * const db_connection, const std::string &journal_name, const std::string &escaped_field_list,
                       const std::string &field_presence) {
    std::vector<std::string> field_names;
    UnescapeList(escaped_field_list, &field_names);

    for (const auto &field_name : field_names) {
        db_connection->queryOrDie("UPDATE metadata_presence_tracer SET field_presence='" + field_presence + "' WHERE journal_id="
                                  "(SELECT id FROM zeder_journals WHERE journal_name=" + db_connection->escapeAndQuoteString(journal_name) + ") "
                                  "AND marc_field_tag=" + db_connection->escapeAndQuoteString(field_name));
        if (db_connection->getNoOfAffectedRows() != 1)
            LOG_WARNING("failed to find a \"" + field_name + "\" for the \"" + journal_name + "\" journal in the database!");
    }
}


void SetFieldsToIgnore(DbConnection * const db_connection, const std::string &journal_name, const std::string &escaped_field_list) {
    SetFieldsToIgnore(db_connection, journal_name, escaped_field_list, "ignore");
}


void SetFieldsToSometimes(DbConnection * const db_connection, const std::string &journal_name, const std::string &escaped_field_list) {
    SetFieldsToIgnore(db_connection, journal_name, escaped_field_list, "sometimes");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    DbConnection db_connection(DbConnection::UBToolsFactory());
    if (std::strcmp(argv[1], "--clear-journals") == 0) {
        if (argc != 3)
            Usage();
        ClearJournals(&db_connection, argv[2]);
    } else if (std::strcmp(argv[1], "--ignore-fields") == 0) {
        if (argc != 4)
            Usage();
        SetFieldsToIgnore(&db_connection, argv[2], argv[3]);
    } else if (std::strcmp(argv[1], "--sometimes-fields") == 0) {
        if (argc != 4)
            Usage();
        SetFieldsToSometimes(&db_connection, argv[2], argv[3]);
    } else
        Usage();

    return EXIT_SUCCESS;
}
