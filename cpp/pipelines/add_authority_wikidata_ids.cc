/** \file add_authority_wikidata_ids.cc
 *  \brief functionality to acquire wikidata id corresponding to their gnds
 *  \author andreas-ub
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "util.h"
#include "StringUtil.h"
#include "MARC.h"
#include <iostream>
#include <fstream>
#include <regex>

//Scenario 1:
// use --create_mapping_file parameter <filepath> to generate the mapping file 
//  of the downloaded dnb authoriy dump (must bei unzipped first)
//  Download from:  https://data.dnb.de/opendata/authorities-person_lds.jsonld.gz 
//  and unzip to e.g. authorities-person_lds_20210613.jsonld
//  output is stdout
//Scenario 2:
// use converted file from scenario 1 from cpp/data to create a map during 
//  pipeline processing. The norm_data_input is extended by wikidata ids where possible
//  and saved to 024 field indicator1:7 where wikidata id is not yet present
//  file can be taken from /mnt/ZE020150/FID-Entwicklung/ub_tools (gnd_to_wiki.txt)
[[noreturn]] void Usage() {
    ::Usage("    :\n"
            "     invocation modes:\n"
            "     1.)   norm_data_marc_input norm_data_marc_output mapping_txt_file\n"
            "     2.)   --create_mapping_file dnb_input_unzipped_file mapping_txt_file\n");
}


void ParseDataDnbFile(std::string input_filename, std::string output_filename) {
    std::ifstream input_file(input_filename);
    std::ofstream output_file(output_filename);
    if (input_file.is_open() and output_file.is_open()) {
        std::string line;
        std::string act_gnd;
        std::string act_name;
        std::string act_wiki;
        bool sameAs_reached(false);
        bool read_preferred_name(false);
        bool read_gnd_id(false);
        while (std::getline(input_file, line)) {
            if (line == "}, {") {
                if (not act_wiki.empty() and not act_name.empty() and not act_wiki.empty()) {
                    output_file << "Name: " << act_name << " GND: " << act_gnd << " Wikidata: " << act_wiki << "\n";
                    std::cout << "Name: " << act_name << " GND: " << act_gnd << " Wikidata: " << act_wiki << "\n";
                }
                act_gnd = "";
                act_name = "";
                act_wiki = "";
                sameAs_reached = false;
                read_gnd_id = true;
            } else if (read_preferred_name) {
                read_preferred_name = false;
                act_name = std::regex_replace(line, std::regex("(value|:|\"|@)"), "");
                act_name = StringUtil::TrimWhite(act_name);
            } else if (StringUtil::Contains(line, "info/gnd/") and read_gnd_id) {
                read_gnd_id = false;
                std::size_t last_slash = line.find_last_of("/");
                act_gnd = line.substr(last_slash + 1);
                act_gnd = std::regex_replace(act_gnd, std::regex("(\\s|,|\")"), "");
            } else if (StringUtil::Contains(line, "www.wikidata.org/entity/") and sameAs_reached) {
                std::size_t last_slash = line.find_last_of("/");
                act_wiki = line.substr(last_slash + 1);
                act_wiki = std::regex_replace(act_wiki, std::regex("(\\s|,|\")"), "");
            } else if (StringUtil::Contains(line, "owl#sameAs")) {
                sameAs_reached = true;
            } else if (StringUtil::Contains(line, "preferredNameForThePerson")) {
                read_preferred_name = true;
            }
        }
        input_file.close();
        output_file.close();
    }
    else 
        LOG_ERROR("input or output files could not be opened");
}

void ParseGndWikidataMappingFile(std::string filename, std::unordered_map<std::string, std::string> * const gnd_to_wikidataid) {
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string line;
        std::string act_gnd;
        std::string act_wiki;
        while (std::getline(file, line)) {
            const std::string NAME = "Name:";
            const std::string GND = "GND:";
            const std::string WIKIDATA = "Wikidata:";
            if (StringUtil::StartsWith(line, NAME) and StringUtil::Contains(line, GND) and StringUtil::Contains(line, WIKIDATA)) {
                act_gnd = line.substr(line.find(GND) + GND.length());
                act_gnd = act_gnd.substr(0, act_gnd.find(WIKIDATA));
                act_wiki = line.substr(line.find(WIKIDATA) + WIKIDATA.length());
                gnd_to_wikidataid->emplace(StringUtil::TrimWhite(act_gnd), StringUtil::TrimWhite(act_wiki));
            }
        }
        file.close();
    }
    else 
        LOG_ERROR("input or output files could not be opened");
}


int Main(int argc, char * argv[]) {

     if (argc != 4)
        Usage();

    const std::string marc_input_filename_or_create_flag(argv[1]);
    const std::string marc_output_filename_or_dnb_input(argv[2]);
    const std::string mapping_txt_filename(argv[3]);

    if (marc_input_filename_or_create_flag == "--create_mapping_file") {
        //e.g. "/..../authorities-person_lds_20210613.jsonld" and /usr/local/ub_tools/cpp/data/gnd_to_wiki.txt
        ParseDataDnbFile(marc_output_filename_or_dnb_input, mapping_txt_filename); 
        return EXIT_SUCCESS;
    }

    std::unordered_map<std::string, std::string> gnd_to_wikidataid;
    ParseGndWikidataMappingFile(mapping_txt_filename, &gnd_to_wikidataid);
    
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename_or_create_flag));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename_or_dnb_input));

    if (unlikely(marc_input_filename_or_create_flag == marc_output_filename_or_dnb_input))
        LOG_ERROR("Norm data input file name equals output file name!");

    while (MARC::Record record = marc_reader.get()->read()) {
        // 035|a (DE-588)118562215
        std::string record_gnd;
        std::string wikidata_id;

        MARC::GetGNDCode(record, &record_gnd);
    
        //record lookup
        if (not record_gnd.empty()) {
            auto gnd_to_wikidataid_iter = gnd_to_wikidataid.find(record_gnd);
            if (gnd_to_wikidataid_iter != gnd_to_wikidataid.end()) {
                wikidata_id = gnd_to_wikidataid_iter->second;
                //std::cout << "Match: " << record_gnd << " --- " << wikidata_id << "\n";
            }
        }
        //record write wikidata id (if not already there)
        bool wikidataid_existing(false);
        auto field_024(record.getFirstField("024"));
        while (field_024 != record.end() and field_024->getTag() == "024") {
            if (not field_024->getFirstSubfieldWithCode('2').empty() and
                field_024->getFirstSubfieldWithCode('2').find("wikidata") != std::string::npos)
            {
                wikidataid_existing = true;
                break;
            }
            ++field_024;
        }
        if (not wikidataid_existing and not wikidata_id.empty()) {
            record.insertField("024", { { 'a', wikidata_id }, { '2', "wikidata" }, {'9', "PipeLineGenerated"} }, /*indicator 1*/ '7');
        }
        marc_writer.get()->write(record);
    }

    return EXIT_SUCCESS;
}
