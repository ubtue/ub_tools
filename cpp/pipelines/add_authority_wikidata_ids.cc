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
        std::string act_wikidata;
        std::string act_wikipedia;
        bool sameAs_reached(false);
        bool read_preferred_name(false);
        bool read_gnd_id(false);
        while (std::getline(input_file, line)) {
            if (line == "}, {") {
                if (not act_wikidata.empty() and not act_name.empty() and not act_wikidata.empty()) {
                    output_file << "Name: " << act_name << " GND: " << act_gnd << " Wikidata: " << act_wikidata << " Wikipedia: " << act_wikipedia << "\n";
                    //std::cout << "Name: " << act_name << " GND: " << act_gnd << " Wikidata: " << act_wikidata << " Wikipedia: " << act_wikipedia << "\n";
                }
                act_gnd = "";
                act_name = "";
                act_wikidata = "";
                act_wikipedia = "";
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
                act_wikidata = line.substr(last_slash + 1);
                act_wikidata = std::regex_replace(act_wikidata, std::regex("(\\s|,|\")"), "");
            } else if (StringUtil::Contains(line, "wikipedia.org/wiki/") and StringUtil::Contains(line, "http") and sameAs_reached) {
                std::size_t first_http = line.find("http");
                act_wikipedia = line.substr(first_http);
                act_wikipedia = std::regex_replace(act_wikipedia, std::regex("(\\s|,|\")"), "");
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

void ParseGndWikidataMappingFile(std::string filename, std::unordered_map<std::string, std::vector<std::string>> * const gnd_to_wikidataid_and_wikipedia_link) 
{
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string line;
        std::string act_gnd;
        std::string act_wikidata;
        std::string act_wikipedia;
        while (std::getline(file, line)) {
            const std::string NAME = "Name:";
            const std::string GND = "GND:";
            const std::string WIKIDATA = "Wikidata:";
            const std::string WIKIPEDIA = "Wikipedia:";
            if (StringUtil::StartsWith(line, NAME) and StringUtil::Contains(line, GND) and StringUtil::Contains(line, WIKIDATA) and StringUtil::Contains(line, WIKIPEDIA)) {
                act_gnd = line.substr(line.find(GND) + GND.length());
                act_gnd = act_gnd.substr(0, act_gnd.find(WIKIDATA));
                act_wikidata = line.substr(line.find(WIKIDATA) + WIKIDATA.length());
                act_wikidata = act_wikidata.substr(0, act_wikidata.find(WIKIPEDIA));
                act_wikipedia = line.substr(line.find(WIKIPEDIA) + WIKIPEDIA.length());
                std::vector<std::string> wiki_elements = { StringUtil::TrimWhite(act_wikidata), StringUtil::TrimWhite(act_wikipedia) };
                gnd_to_wikidataid_and_wikipedia_link->emplace(StringUtil::TrimWhite(act_gnd), wiki_elements);
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

    std::unordered_map<std::string, std::vector<std::string>> gnd_to_wikielements;
    ParseGndWikidataMappingFile(mapping_txt_filename, &gnd_to_wikielements);
    
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename_or_create_flag));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename_or_dnb_input));

    if (unlikely(marc_input_filename_or_create_flag == marc_output_filename_or_dnb_input))
        LOG_ERROR("Norm data input file name equals output file name!");

    while (MARC::Record record = marc_reader.get()->read()) {
        // 035|a (DE-588)118562215
        std::string record_gnd;
        std::string wikidata_id;
        std::string wikipedia_link;
        std::vector<std::string> wiki_elements;

        MARC::GetGNDCode(record, &record_gnd);
        MARC::GetWikidataId(record, &wikidata_id);

        if (not wikidata_id.empty())
            continue;
    
        //record lookup
        if (not record_gnd.empty()) {
            auto gnd_to_wikielements_iter = gnd_to_wikielements.find(record_gnd);
            if (gnd_to_wikielements_iter != gnd_to_wikielements.end()) {
                wiki_elements = gnd_to_wikielements_iter->second;
                if (wiki_elements.size() > 0)
                    wikidata_id = wiki_elements[0];
                if (wiki_elements.size() > 1)
                    wikipedia_link = wiki_elements[1];
            }
        }
        
        if (not wikidata_id.empty())
            record.insertField("024", { { 'a', wikidata_id }, { '2', "wikidata" }, { '9', "PipeLineGenerated" } }, /*indicator 1*/ '7');
        if (not wikipedia_link.empty())
            record.insertField("670", { { 'a', "Wikipedia" }, { 'u', wikipedia_link }, { '9', "PipeLineGenerated" } });
        
        marc_writer.get()->write(record);
    }

    return EXIT_SUCCESS;
}
