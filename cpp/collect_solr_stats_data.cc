/* \brief A tool to collect various counts of subsets of Solr records.
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
#include <cstdlib>
#include <ctime>
#include "DbConnection.h"
#include "DnsUtil.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "JSON.h"
#include "Solr.h"
#include "TimeUtil.h"
#include "util.h"


namespace {


const std::string RELBIB_EXTRA(" AND is_religious_studies:1");


void Usage() {
    std::cerr << "Usage: " << ::progname << " system_type output_file\n";
    std::exit(EXIT_FAILURE);
}


void IssueQueryAndWriteOutput(const std::string &query, const std::string &system_type, const std::string &category,
                              const std::string &variable, DbConnection * const db_connection) {
    static const time_t JOB_START_TIME(std::time(nullptr));
    static const std::string HOSTNAME(DnsUtil::GetHostname());

    std::string json_result, err_msg;
    if (not Solr::Query(query, /* fields = */ "", &json_result, &err_msg, Solr::DEFAULT_HOST, Solr::DEFAULT_PORT,
                        /* timeout in seconds = */ Solr::DEFAULT_TIMEOUT, Solr::JSON, /* max_no_of_rows = */ 0))
        LOG_ERROR("Solr query \"" + query + "\" failed! (" + err_msg + ")");

    JSON::Parser parser(json_result);
    std::shared_ptr<JSON::JSONNode> tree_root;
    if (not parser.parse(&tree_root))
        LOG_ERROR("JSON parser failed: " + parser.getErrorMessage());

    const time_t NOW(std::time(nullptr));
    db_connection->queryOrDie("INSERT INTO solr SET id_lauf=" + std::to_string(JOB_START_TIME) + ", timestamp='"
                              + TimeUtil::TimeTToZuluString(NOW) + "', Quellrechner='" + HOSTNAME + "', Zielrechner='" + HOSTNAME
                              + "', Systemtyp='" + system_type + "', Kategorie='" + category + "', Unterkategorie='" + variable
                              + "', value=" + std::to_string(JSON::LookupInteger("/response/numFound", tree_root)));
}


void CollectGeneralStats(const std::string &system_type, DbConnection * const db_connection) {
    const std::string EXTRA(system_type == "relbib" ? RELBIB_EXTRA : "");
    IssueQueryAndWriteOutput("*:*" + EXTRA, system_type, "Gesamt", "Gesamttreffer", db_connection);
    IssueQueryAndWriteOutput("format:Book" + EXTRA, system_type, "Format", "Buch", db_connection);
    IssueQueryAndWriteOutput("format:Article" + EXTRA, system_type, "Format", "Artikel", db_connection);
    IssueQueryAndWriteOutput("mediatype:Electronic" + EXTRA, system_type, "Medientyp", "elektronisch", db_connection);
    IssueQueryAndWriteOutput("mediatype:Non-Electronic" + EXTRA, system_type, "Medientyp", "non-elektronisch", db_connection);
}


void CollectKrimDokSpecificStats(DbConnection * const db_connection) {
    IssueQueryAndWriteOutput("language:German", "krimdok", "Sprache", "Deutsch", db_connection);
    IssueQueryAndWriteOutput("language:English", "krimdok", "Sprache", "Englisch", db_connection);
}


void EmitNotationStats(const char notation_group, const std::string &system_type, const std::string &label,
                       DbConnection * const db_connection) {
    const std::string EXTRA(system_type == "relbib" ? RELBIB_EXTRA : "");
    IssueQueryAndWriteOutput("ixtheo_notation:" + std::string(1, notation_group) + "* AND publishDate:[1975 TO 2000]" + EXTRA, system_type,
                             "IxTheo Notationen", label + "(Alle Medienarten, 1975-2000)", db_connection);
    IssueQueryAndWriteOutput("ixtheo_notation:" + std::string(1, notation_group) + "* AND publishDate:[2001 TO *]" + EXTRA, system_type,
                             "IxTheo Notationen", label + "(Alle Medienarten, 2001-heute)", db_connection);
    IssueQueryAndWriteOutput(
        "ixtheo_notation:" + std::string(1, notation_group) + "* AND publishDate:[1975 TO 2000] AND format:Book" + EXTRA, system_type,
        "IxTheo Notationen", label + "(Bücher, 1975-2000)", db_connection);
    IssueQueryAndWriteOutput("ixtheo_notation:" + std::string(1, notation_group) + "* AND publishDate:[2001 TO *] AND format:Book" + EXTRA,
                             system_type, "IxTheo Notationen", label + "(Bücher, 2001-heute)", db_connection);
    IssueQueryAndWriteOutput(
        "ixtheo_notation:" + std::string(1, notation_group) + "* AND publishDate:[1975 TO 2000] AND format:Article" + EXTRA, system_type,
        "IxTheo Notationen", label + "(Bücher, 1975-2000)", db_connection);
    IssueQueryAndWriteOutput(
        "ixtheo_notation:" + std::string(1, notation_group) + "* AND publishDate:[2001 TO *] AND format:Article" + EXTRA, system_type,
        "IxTheo Notationen", label + "(Aufsätze, 2001-heute)", db_connection);
}


void CollectIxTheoOrRelBibSpecificStats(const std::string &system_type, DbConnection * const db_connection) {
    const std::string EXTRA(system_type == "relbib" ? RELBIB_EXTRA : "");
    IssueQueryAndWriteOutput("dewey-raw:*" + EXTRA, system_type, "DDC", "Anzahl der Datensätze", db_connection);
    IssueQueryAndWriteOutput("rvk:*" + EXTRA, system_type, "RVK", "Anzahl der Datensätze", db_connection);
    IssueQueryAndWriteOutput("is_open_access:open-access" + EXTRA, system_type, "Open Access", "ja", db_connection);
    IssueQueryAndWriteOutput("is_open_access:non-open-access" + EXTRA, system_type, "Open Access", "nein", db_connection);

    IssueQueryAndWriteOutput("language:German" + EXTRA, system_type, "Sprache", "Deutsch", db_connection);
    IssueQueryAndWriteOutput("language:English" + EXTRA, system_type, "Sprache", "Englisch", db_connection);
    IssueQueryAndWriteOutput("language:French" + EXTRA, system_type, "Sprache", "Französisch", db_connection);
    IssueQueryAndWriteOutput("language:Italian" + EXTRA, system_type, "Sprache", "Italienisch", db_connection);
    IssueQueryAndWriteOutput("language:Latin" + EXTRA, system_type, "Sprache", "Latein", db_connection);
    IssueQueryAndWriteOutput("language:Spanish" + EXTRA, system_type, "Sprache", "Spanisch", db_connection);
    IssueQueryAndWriteOutput("language:Dutch" + EXTRA, system_type, "Sprache", "Holländisch", db_connection);
    IssueQueryAndWriteOutput("language:\"Ancient Greek\"" + EXTRA, system_type, "Sprache", "Altgriechisch", db_connection);
    IssueQueryAndWriteOutput("language:Hebrew" + EXTRA, system_type, "Sprache", "Hebräisch", db_connection);
    IssueQueryAndWriteOutput("language:Portugese" + EXTRA, system_type, "Sprache", "Portugiesisch", db_connection);

    IssueQueryAndWriteOutput("ixtheo_notation:*" + EXTRA, system_type, "IxTheo Notationen", "Mit Notation", db_connection);
    IssueQueryAndWriteOutput("-ixtheo_notation:*" + EXTRA, system_type, "IxTheo Notationen", "Ohne Notation", db_connection);
    EmitNotationStats('A', system_type, "Religionswissenschaft allgemein", db_connection);
    EmitNotationStats('B', system_type, "Einzelne Religionen", db_connection);
    EmitNotationStats('C', system_type, "Christentum", db_connection);
    EmitNotationStats('F', system_type, "Christliche Theologie", db_connection);
    EmitNotationStats('H', system_type, "Bibel; Bibelwissenschaft", db_connection);
    EmitNotationStats('K', system_type, "Kirchen- und Theologiegeschichte; Konfessionskunde", db_connection);
    EmitNotationStats('N', system_type, "Systematische Theologie", db_connection);
    EmitNotationStats('R', system_type, "Praktische Theologie", db_connection);
    EmitNotationStats('S', system_type, "Kirchenrecht", db_connection);
    EmitNotationStats('T', system_type, "(Profan-) Geschichte", db_connection);
    EmitNotationStats('V', system_type, "Philosophie", db_connection);
    EmitNotationStats('X', system_type, "Recht allgemein", db_connection);
    EmitNotationStats('Z', system_type, "Sozialwissenschaften", db_connection);
}


} // unnamed namespace


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string system_type(argv[1]);
    if (system_type != "ixtheo" and system_type != "relbib" and system_type != "krimdok")
        LOG_ERROR("system type must be one of {ixtheo, relbib, krimdok}!");

    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(argv[2]));

    try {
        const IniFile ini_file;
        DbConnection db_connection(DbConnection::MySQLFactory(ini_file));

        CollectGeneralStats(system_type, &db_connection);
        if (system_type == "krimdok")
            CollectKrimDokSpecificStats(&db_connection);
        else
            CollectIxTheoOrRelBibSpecificStats(system_type, &db_connection);
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}
