/** \file oai_pmh_harvester.cc
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Logger.h"
#include "Marc21OaiPmhClient.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " logfile_path ini_file_path section_name [harvest_set] marc_output\n"
              << "       The ini file section must contain the entries \"repository_name\" \"base_url\",\n"
              << "       \"metadata_prefix\", and \"harvest_mode\" where \"harvest_mode\" must nbe either\n"
              << "       \"FULL\" or \"INCREMENTAL\".\n\n";

    std::exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 5 and argc != 6)
        Usage();

    const std::string ini_filename(argv[2]);
    const std::string ini_section_name(argv[3]);
    const std::string harvest_set(argc == 6 ? argv[4] : "");
    const std::string marc_output_filename(argc == 6 ? argv[5] : argv[4]);

    try {
        const std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename));

        const IniFile ini_file(ini_filename);
        Marc21OaiPmhClient oai_pmh_client(ini_file, ini_section_name, marc_writer.get());

        std::string xml_response, err_msg;
        if (not oai_pmh_client.identify(&xml_response, &err_msg))
            Error("\"Identify\" failed: " + err_msg);
        std::cout << xml_response;

        Logger logger(argv[1]);
        if (harvest_set.empty())
            oai_pmh_client.harvest(/* verbosity = */ 3, &logger);
        else
            oai_pmh_client.harvest(harvest_set, /* verbosity = */ 3, &logger);
        std::cout << "Harvested " << oai_pmh_client.getRecordCount() << " record(s).\n";
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
