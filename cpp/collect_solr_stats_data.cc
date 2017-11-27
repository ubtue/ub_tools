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
    const std::string BASE_QUERY(system_type != "relbib" ? "http://localhost:8080?wt=json"
                                                         : "http://localhost:8080?wt=json&fq=is_religious_studies:1");
    IssueQueryAndWriteOutput(BASE_QUERY, "Gesamt", "Gesamttreffer", system_type, output);
    IssueQueryAndWriteOutput(BASE_QUERY + "&format:Book", "Format", "Buch", system_type, output);
    IssueQueryAndWriteOutput(BASE_QUERY + "&format:Article", "Format", "Artikel", system_type, output);
    IssueQueryAndWriteOutput(BASE_QUERY + "&mediatype:Electronic", "Medientyp", "elektronisch", system_type, output);
    IssueQueryAndWriteOutput(BASE_QUERY + "&mediatype:Non-Electronic", "Medientyp", "non-elektronisch", system_type, output);
}


void CollectKrimDokSpecificStats(File * const /*output*/) {
}


void CollectIxTheoOrRelBibSpecificStats(const std::string &/*system_type*/, File * const /*output*/) {
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
