/* \brief A tool to collect various counts of subsets of Solr records.
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
#include <cstdlib>
#include "FileUtil.h"
#include "JSON.h"
#include "Solr.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace {


void Usage() {
    std::cerr << "Usage: " << ::progname << " system_type output_file\n";
    std::exit(EXIT_FAILURE);
}


void IssueQueryAndWriteOutput(const std::string &query, const std::string &system_type, const std::string &category,
                              const std::string &variable, File * const output)
{
    std::string json_result;
    if (not Solr::Query(query, /* fields = */"", &json_result, "localhost:8080", /* timeout in seconds = */10, Solr::JSON))
        logger->error("in IssueQueryAndWriteOutput: Solr query \"" + query + "\" failed!");

    JSON::Parser parser(json_result);
    JSON::JSONNode *tree_root;
    if (not parser.parse(&tree_root))
        logger->error("in IssueQueryAndWriteOutput: JSON parser failed: " + parser.getErrorMessage());

    *output << '"' << TextUtil::CSVEscape(system_type) << "\",\"" << TextUtil::CSVEscape(category) << "\",\""
            << TextUtil::CSVEscape(variable) << "\"," << JSON::LookupInteger("/response/numFound", tree_root) << ",\""
            << TextUtil::CSVEscape(TimeUtil::GetCurrentDateAndTime()) << "\"\n";
    delete tree_root;
}


void CollectGeneralStats(const std::string &system_type, File * const output) {
    const std::string EXTRA(system_type == "relbib" ? "&fq=is_religious_studies:1" : "");
    IssueQueryAndWriteOutput("*:*" + EXTRA, system_type, "Gesamt", "Gesamttreffer", output);
    IssueQueryAndWriteOutput("format:Book" + EXTRA, system_type, "Format", "Buch", output);
    IssueQueryAndWriteOutput("format:Article" + EXTRA, system_type, "Format", "Artikel", output);
    IssueQueryAndWriteOutput("mediatype:Electronic" + EXTRA, system_type, "Medientyp", "elektronisch", output);
    IssueQueryAndWriteOutput("mediatype:Non-Electronic" + EXTRA, system_type, "Medientyp", "non-elektronisch", output);
    IssueQueryAndWriteOutput("is_open_access:open-access" + EXTRA, system_type, "Open Access", "ja", output);
    IssueQueryAndWriteOutput("is_open_access:non-open-access" + EXTRA, system_type, "Open Access", "nein", output);
}


void CollectKrimDokSpecificStats(File * const output) {
    IssueQueryAndWriteOutput("language:German", "krimdok", "Sprache", "Deutsch", output);
    IssueQueryAndWriteOutput("language:English", "krimdok", "Sprache", "Englisch", output);
}


void EmitNotationStats(const char notation_group, const std::string &system_type, const std::string &label, File * const output) {
    const std::string EXTRA(system_type == "relbib" ? "&fq=is_religious_studies:1" : "");
    IssueQueryAndWriteOutput("ixtheo_notation:" + std::string(1, notation_group) + "*+AND+publishDate:[1975+TO+2000]" + EXTRA,
                             system_type, "IxTheo Notationen", label + "(Alle Medienarten, 1975-2000)", output);
    IssueQueryAndWriteOutput("ixtheo_notation:" + std::string(1, notation_group) + "*+AND+publishDate:[2001+TO+*]" + EXTRA,
                             system_type, "IxTheo Notationen", label + "(Alle Medienarten, 2001-heute)", output);
    IssueQueryAndWriteOutput("ixtheo_notation:" + std::string(1, notation_group)
                             + "*+AND+publishDate:[1975+TO+2000]+AND+format:Book" + EXTRA, system_type, "IxTheo Notationen",
                             label + "(Bücher, 1975-2000)", output);
    IssueQueryAndWriteOutput("ixtheo_notation:" + std::string(1, notation_group) + "*+AND+publishDate:[2001+TO+*]+AND+format:Book"
                             + EXTRA, system_type, "IxTheo Notationen", label + "(Bücher, 2001-heute)", output);
    IssueQueryAndWriteOutput("ixtheo_notation:" + std::string(1, notation_group)
                             + "*+AND+publishDate:[1975+TO+2000]+AND+format:Article" + EXTRA, system_type, "IxTheo Notationen",
                             label + "(Bücher, 1975-2000)", output);
    IssueQueryAndWriteOutput("ixtheo_notation:" + std::string(1, notation_group)
                             + "*+AND+publishDate:[2001+TO+*]+AND+format:Article" + EXTRA, system_type, "IxTheo Notationen",
                             label + "(Aufsätze, 2001-heute)", output);
}


void CollectIxTheoOrRelBibSpecificStats(const std::string &system_type, File * const output) {
    const std::string EXTRA(system_type == "relbib" ? "&fq=is_religious_studies:1" : "");
    IssueQueryAndWriteOutput("dewey-raw:*" + EXTRA, system_type, "DDC", "Anzahl der Datensätze", output);
    IssueQueryAndWriteOutput("rvk:*" + EXTRA, system_type, "RVK", "Anzahl der Datensätze", output);

    IssueQueryAndWriteOutput("language:German" + EXTRA, system_type, "Sprache", "Deutsch", output);
    IssueQueryAndWriteOutput("language:English" + EXTRA, system_type, "Sprache", "Englisch", output);
    IssueQueryAndWriteOutput("language:French" + EXTRA, system_type, "Sprache", "Französisch", output);
    IssueQueryAndWriteOutput("language:Italian" + EXTRA, system_type, "Sprache", "Italienisch", output);
    IssueQueryAndWriteOutput("language:Latin" + EXTRA, system_type, "Sprache", "Latein", output);
    IssueQueryAndWriteOutput("language:Spanish" + EXTRA, system_type, "Sprache", "Spanisch", output);
    IssueQueryAndWriteOutput("language:Dutch" + EXTRA, system_type, "Sprache", "Holländisch", output);
    IssueQueryAndWriteOutput("language:\"Ancient Greek\"" + EXTRA, system_type, "Sprache", "Altgriechisch", output);
    IssueQueryAndWriteOutput("language:Hebrew" + EXTRA, system_type, "Sprache", "Hebräisch", output);
    IssueQueryAndWriteOutput("language:Portugese" + EXTRA, system_type, "Sprache", "Portugiesisch", output);

    IssueQueryAndWriteOutput("ixtheo_notation:*" + EXTRA, system_type, "IxTheo Notationen", "Mit Notation", output);
    IssueQueryAndWriteOutput("-ixtheo_notation:*" + EXTRA, system_type, "IxTheo Notationen", "Ohne Notation", output);
    EmitNotationStats('A', system_type, "Religionswissenschaft allgemein", output);
    EmitNotationStats('B', system_type, "Einzelne Religionen", output);
    EmitNotationStats('C', system_type, "Christentum", output);
    EmitNotationStats('F', system_type, "Christliche Theologie", output);
    EmitNotationStats('H', system_type, "Bibel; Bibelwissenschaft", output);
    EmitNotationStats('K', system_type, "Kirchen- und Theologiegeschichte; Konfessionskunde", output);
    EmitNotationStats('N', system_type, "Systematische Theologie", output);
    EmitNotationStats('R', system_type, "Praktische Theologie", output);
    EmitNotationStats('S', system_type, "Kirchenrecht", output);
    EmitNotationStats('T', system_type, "(Profan-) Geschichte", output);
    EmitNotationStats('V', system_type, "Philosophie", output);
    EmitNotationStats('X', system_type, "Recht allgemein", output);
    EmitNotationStats('Z', system_type, "Sozialwissenschaften", output);
}


} // unnamed namespace


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string system_type(argv[1]);
    if (system_type != "ixtheo" and system_type != "relbib" and system_type != "krimdok")
        logger->error("system type must be one of {ixtheo, relbib, krimdok}!");

    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(argv[2]));

    try {
        CollectGeneralStats(system_type, output.get());
        if (system_type == "krimdok")
            CollectKrimDokSpecificStats(output.get());
        else
            CollectIxTheoOrRelBibSpecificStats(system_type, output.get());
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
