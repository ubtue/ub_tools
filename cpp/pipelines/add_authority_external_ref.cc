/** \file add_authority_wikidata_ids.cc
 *  \brief functionality to acquire wikidata id corresponding to their gnds
 *  \author andreas-ub
 *  \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 *  \copyright 2021-2022 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <chrono>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <nlohmann/json.hpp>
#include "BeaconFile.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"

/**
 * Generating input file must use the jq program. Since the .jsonld file is large enough than it is a must to parse it as a stream and pipe
 * it using grep. The complete command for this task is: jq -c --stream '.' < authorities-gnd-person_lds.jsonld |grep -E
 * 'https\\:/\\/d-nb\\.info\\/gnd\\/|wikidata|wikipedia'
 */
[[noreturn]] void Usage() {
    std::cerr << "Usage: " << progname
              << "  --create_mapping_file  input_txt_file  output_csv_file"
                 "\n\n"
                 "\t- input_txt_file: The essential information from authorities-gnd-person_lds.jsonld.\n"
                 "\tGenerate the input file using the 'jq' program. \n"
                 "\t- output_csv_file: the gnd_to_wiki file to write to, it is a csv with ';' as delimiter.\n";

    std::exit(EXIT_FAILURE);
}


struct GNDStructure {
    std::string gnd_id, wikidata_personal_entity_id, wikipedia_personal_address;
};

bool IsThisCloseBracketForId(const std::string &url) {
    int url_length = url.length();
    std::string is_about(url.substr(url_length - 5, 5));
    if (is_about.compare("about") == 0)
        return true;

    return false;
}

bool DoesTheUrlAddressMatch(const std::string &url_based, const std::string &url_comp) {
    const int base_string_length = url_based.length();
    const std::string sub_string_url_comp = url_comp.substr(0, base_string_length);
    if (url_based.compare(sub_string_url_comp) == 0)
        return true;

    return false;
}

bool GenerateGNDAuthorityExternalRef(char *argv[]) {
    const auto load_file_start(std::chrono::high_resolution_clock::now());
    std::ifstream input_file(argv[2]);

    if (!input_file.is_open()) {
        LOG_ERROR("can't open input file");
        return false;
    }

    const auto csv_file(FileUtil::OpenOutputFileOrDie(argv[3]));


    const std::string dnb_address("https://d-nb.info/gnd/");
    const std::string wikidata_address("http://www.wikidata.org/entity/");
    const std::string wikipedia_address("https://de.wikipedia.org/wiki/");
    std::string gnd_id;
    bool is_start_group = false;
    GNDStructure gnd_data;
    int top_level_number(-1), second_level_number(-1), total_numbers_of_gnd_id_generated(0), total_line_parsed(0),
        total_number_of_wikidata(0), total_number_of_wikipedia(0);

    const int dnb_add_str_length(dnb_address.length()), wikidata_address_str_length(wikidata_address.length());

    std::string line, id_annotaton(""), second_element_of_array;
    nlohmann::json line_parsed;
    std::string gnd_id_temp_string;
    std::string wikidata_temp_string;
    std::string tmp_id;


    while (std::getline(input_file, line)) {
        line_parsed = nlohmann::json::parse(line);
        // get information on first element of array
        if (line_parsed[0].is_array() && line_parsed[0][2].is_string()) {
            id_annotaton = line_parsed[0][2].get<std::string>();
            if (id_annotaton.compare("@id") == 0) {
                // the second element of array is not an array nor object
                if (!line_parsed[1].is_structured()) {
                    // if id without about, this means the beginning of group or opening bracket.
                    if (!IsThisCloseBracketForId(line_parsed[1])) {
                        // get gnd id, set is_start_group to true, set top_level_number, set second_level_number
                        // tmp_id = nlohmann::to_string(line_parsed[1]);
                        if (line_parsed[1].is_string()) {
                            top_level_number = line_parsed[0][0].get<int>();
                            second_level_number = line_parsed[0][1].get<int>();
                            is_start_group = true;
                            gnd_id_temp_string = line_parsed[1].get<std::string>();
                            gnd_data.gnd_id =
                                gnd_id_temp_string.substr(dnb_add_str_length, (gnd_id_temp_string.length() - dnb_add_str_length));

                            ++total_numbers_of_gnd_id_generated;
                        }
                    }
                }
                // if id -> about, this means the last of group or this is the closing bracket.
                // then print the accumulation of last result data and reset all info
                if (IsThisCloseBracketForId(line_parsed[1])) {
                    csv_file->write(TextUtil::CSVEscape(gnd_data.gnd_id) + ";");
                    csv_file->write(TextUtil::CSVEscape(gnd_data.wikidata_personal_entity_id) + ";");
                    csv_file->write(TextUtil::CSVEscape(gnd_data.wikipedia_personal_address) + "\n");
                    top_level_number = -1;
                    second_level_number = -1;
                    is_start_group = false;
                    gnd_id_temp_string = "";
                    gnd_data = {};
                }
            }

            if (is_start_group) {
                // std::cout << top_level_number << " , " << second_level_number << std::endl;
                if (line_parsed[0][0].get<int>() == top_level_number && line_parsed[0][1].get<int>() == second_level_number) {
                    if (!line_parsed[1].is_structured()) {
                        if (line_parsed[1].is_string()) {
                            if (DoesTheUrlAddressMatch(wikipedia_address, line_parsed[1].get<std::string>())) {
                                gnd_data.wikipedia_personal_address = line_parsed[1].get<std::string>();

                                ++total_number_of_wikipedia;
                            }

                            // if wikidata
                            if (DoesTheUrlAddressMatch(wikidata_address, line_parsed[1].get<std::string>())) {
                                wikidata_temp_string = nlohmann::to_string(line_parsed[1]);
                                gnd_data.wikidata_personal_entity_id = wikidata_temp_string.substr(
                                    wikidata_address_str_length + 1, (wikidata_temp_string.length() - (wikidata_address_str_length + 2)));

                                ++total_number_of_wikidata;
                            }
                        }
                    }
                }
            }
        }
        std::cout << "\r"
                  << "Parsed: " << total_line_parsed << " line(s), "
                  << " Total GND-ID: " << total_numbers_of_gnd_id_generated << ", Total GND with Wikidata: " << total_number_of_wikidata
                  << ", Total GND with Wikipedia: " << total_number_of_wikipedia;
        //   << "GND-ID url: " << tmp_id;
        std::cout.flush();
        ++total_line_parsed;
    }

    const auto load_file_end(std::chrono::high_resolution_clock::now());
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(load_file_end - load_file_start);

    std::cout << std::endl
              << "Total GND-ID: " << total_numbers_of_gnd_id_generated << std::endl
              << "Total GND with Wikidata: " << total_number_of_wikidata << std::endl
              << "Total GND with Wikipedia: " << total_number_of_wikipedia << std::endl;
    std::cout << "Total time of computation: " << duration.count() << " second(s)" << std::endl;

    input_file.close();
    return true;
}

int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const std::string marc_input_filename_or_create_flag(argv[1]);
    const std::string marc_output_filename_or_dnb_input(argv[2]);
    const std::string mapping_txt_filename(argv[3]);

    if (marc_input_filename_or_create_flag == "--create_mapping_file") {
        if (argc != 4)
            Usage();

        if (GenerateGNDAuthorityExternalRef(argv))
            return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
