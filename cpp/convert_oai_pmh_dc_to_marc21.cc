/** \brief Parses BASE-enhanced OAI-PMH Dublin Core XML and generates MARC-21 data.
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
#include <map>
#include <cstdlib>
#include "FileUtil.h"
#include "MarcUtil.h"
#include "MiscUtil.h"
#include "MarcXmlWriter.h"
#include "SimpleXmlParser.h"
#include "StringUtil.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--verbose] --output-format=(marc_binary|marc_xml) config_file oai_pmh_dc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void LoadConfig(File * const input, std::map<std::string, std::string> * const xml_tag_to_marc_entry_map) {
    xml_tag_to_marc_entry_map->clear();
    
    while (not input->eof()) {
        std::string line(input->getline());

        // Process optional comment:
        const std::string::size_type first_hash_pos(line.find('#'));
        if (first_hash_pos != std::string::npos)
            line.resize(first_hash_pos);

        StringUtil::TrimWhite(&line);
        if (line.empty())
            continue;
    }
}


/** Generates a PPN by conting down from the largest possible PPN. */
std::string GeneratePPN() {
    static unsigned next_ppn(99999999);
    const std::string ppn_without_checksum_digit(StringUtil::ToString(next_ppn, /* radix = */10, /* width = */8));
    --next_ppn;
    return ppn_without_checksum_digit + MiscUtil::GeneratePPNChecksumDigit(ppn_without_checksum_digit);
}


enum OutputFormat { MARC_BINARY, MARC_XML };


void ProcessRecords(const bool verbose, const OutputFormat output_format, File * const input,
                    File * const output)
{
    MarcXmlWriter *xml_writer;
    if (output_format == MARC_XML)
        xml_writer = new MarcXmlWriter(output);
    else
        xml_writer = nullptr;
    
    SimpleXmlParser::Type type;
    std::string data;
    std::map<std::string, std::string> attrib_map;
    SimpleXmlParser xml_parser(input);
    MarcUtil::Record record;
    unsigned record_count(0);
    while (xml_parser.getNext(&type, &attrib_map, &data)) {
        switch (type) {
        case SimpleXmlParser::END_OF_DOCUMENT:
            if (verbose)
                std::cout << "Found " << record_count << " record(s) in the XML input stream.\n";
            delete xml_writer;
            return;
        case SimpleXmlParser::OPENING_TAG:
            if (data == "record") {
                record = MarcUtil::Record();
                record.insertField("001", GeneratePPN());
            }
            break;
        case SimpleXmlParser::CLOSING_TAG:
            if (data == "record") {
                (xml_writer == nullptr) ? record.write(output) : record.write(xml_writer);
                ++record_count;
            }
            break;
        case SimpleXmlParser::CHARACTERS:
            break;
        default:
            /* Intentionally empty! */;
        }
    }

    Error("XML parsing error: " + xml_parser.getLastErrorMessage());
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5 and argc != 6)
        Usage();

    bool verbose(false);
    if (argc == 6) {
        if (std::strcmp(argv[1], "--verbose") != 0)
            Usage();
        verbose = true;
        --argc, ++argv;
    }

    OutputFormat output_format;
    if (std::strcmp(argv[1], "--output-format=marc_binary") == 0)
        output_format = MARC_BINARY;
    else if (std::strcmp(argv[1], "--output-format=marc_xml") == 0)
        output_format = MARC_XML;
    else
        Usage();

    const std::unique_ptr<File> config_input(FileUtil::OpenInputFileOrDie(argv[2]));
    const std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(argv[3]));
    const std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(argv[4]));

    try {
        std::map<std::string, std::string> xml_tag_to_marc_entry_map;
        LoadConfig(config_input.get(), &xml_tag_to_marc_entry_map);
        ProcessRecords(verbose, output_format, input.get(), output.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
