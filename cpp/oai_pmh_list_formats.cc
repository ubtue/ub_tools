/** \file oai_pmh_list_formats.cc
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
#include "Marc21OaiPmhClient.h"
#include "MarcWriter.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " ini_file_path ini_file_section_name\n"
              << "       The ini file section must contain the entries \"repository_name\" \"base_url\",\n"
              << "       \"metadata_prefix\", and \"harvest_mode\" where \"harvest_mode\" must be either\n"
              << "       \"FULL\" or \"INCREMENTAL\".\n\n";

    std::exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string ini_filename(argv[1]);
    const std::string ini_section_name(argv[2]);

    try {
        const IniFile ini_file(ini_filename);
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory("/dev/null", MarcWriter::BINARY));
        Marc21OaiPmhClient oai_pmh_client(ini_file, ini_section_name, marc_writer.get());

        std::vector<OaiPmh::Client::MetadataFormatDescriptor> metadata_format_list;
        std::string err_msg;
        if (not oai_pmh_client.listMetadataFormats(&metadata_format_list, &err_msg))
            Error("oai_pmh_client.listMetadataFormats: " + err_msg);
        std::cerr << "Found " << metadata_format_list.size() << " supported metadata prefix(es),\n";
        for (const auto &metadata_format : metadata_format_list)
            std::cerr << '\t' << metadata_format.metadata_prefix_ << '\n';
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
