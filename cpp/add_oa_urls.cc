/** \file    add_oa_urls.cc
 *  \brief   Add additional urls for oa_access of items
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2018, Library of the University of Tübingen

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
#include <memory>
#include <unordered_map>
#include "Compiler.h"
#include "FileUtil.h"
#include "JSON.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname \
              << " doi_to_url_map.json marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void CreateDoiToUrlMap(const std::string &map_filename, std::unordered_map<std::string, std::string> * const doi_to_url) {
    std::string json_document;
    if (not FileUtil::ReadString(map_filename, &json_document))
        LOG_ERROR("Could not read in " + map_filename);
  
    JSON::Parser json_parser(json_document);
    std::shared_ptr<JSON::JSONNode> entries;
    if (not json_parser.parse(&entries))
        LOG_ERROR("Could not properly parse \"" + map_filename + ": " + json_parser.getErrorMessage());
    
    std::shared_ptr<JSON::ArrayNode> entry_array(JSON::JSONNode::CastToArrayNodeOrDie("", entries));
    for (const auto &entry : *entry_array) {
        const std::string doi(LookupString("/doi", entry));
        const std::string url(LookupString("/best_oa_location/url", entry));
        if (not (doi.empty() or url.empty()))
            doi_to_url->emplace(doi, url);
        else
            LOG_ERROR("Either doi or url missing");
    }
} 


bool AlreadyHasIdenticalUrl(const MARC::Record &record, const std::string &url) {
    for (const auto &field : record.getTagRange("856")) {
        if (field.hasSubfieldWithValue('u', url))
            return true;        
    }
    return false;
}


void Augment856(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                const std::unordered_map<std::string, std::string> &doi_to_url)
{
    while (MARC::Record record = marc_reader->read()) {
        for (const auto &field : record.getTagRange("024")) {
            if (field.hasSubfieldWithValue('2', "doi")) {
                const std::string doi(field.getFirstSubfieldWithCode('a'));
                const auto doi_and_url(doi_to_url.find(doi));
                if (doi_and_url != doi_to_url.cend()) {
                    const std::string url(doi_and_url->second);
                    if (not AlreadyHasIdenticalUrl(record, url))
                        record.insertField("856", { { 'u', url }, { 'z', "unpaywall" } });
                }
            }
        }
        marc_writer->write(record);
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
   if (argc != 4)
       Usage();
   
   std::unordered_map<std::string, std::string> doi_to_url;
   CreateDoiToUrlMap(argv[1], &doi_to_url);
   std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[2]));
   std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[3]));
   Augment856(marc_reader.get(), marc_writer.get(), doi_to_url);

   return EXIT_SUCCESS;
}

