/** \file    add_oa_urls.cc
 *  \brief   Add additional urls for oa_access of items
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2018-2019, Library of the University of Tübingen

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


struct oadoi_info {
    std::string url_;
    std::string evidence_;
    std::string host_type_;

public:
    oadoi_info(const std::string url, const std::string evidence, const std::string host_type)
        : url_(url), evidence_(evidence), host_type_(host_type) { }
};


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " doi_to_url_map.json marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void CreateDoiToUrlMap(const std::string &map_filename, std::unordered_map<std::string, oadoi_info> * const doi_to_oainfo) {
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
        const std::string evidence(LookupString("/best_oa_location/evidence", entry));
        const std::string host_type(LookupString("/best_oa_location/host_type", entry));
        if (not(doi.empty() or url.empty()))
            doi_to_oainfo->emplace(doi, oadoi_info(url, evidence, host_type));
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
                const std::unordered_map<std::string, oadoi_info> &doi_to_oainfo) {
    while (MARC::Record record = marc_reader->read()) {
        bool flag_as_open_access_resource(false);
        for (const auto &field : record.getTagRange("024")) {
            if (field.hasSubfieldWithValue('2', "doi")) {
                const std::string doi(field.getFirstSubfieldWithCode('a'));
                const auto doi_and_info(doi_to_oainfo.find(doi));
                if (doi_and_info != doi_to_oainfo.cend()) {
                    const auto oainfo(doi_and_info->second);
                    const std::string url(oainfo.url_);
                    const std::string evidence(oainfo.evidence_);
                    const std::string host_type(oainfo.host_type_);
                    if (not AlreadyHasIdenticalUrl(record, url))
                        record.insertField("856", { { 'u', url },
                                                    { 'x', "unpaywall" },
                                                    { 'z', "Vermutlich kostenfreier Zugang" },
                                                    { 'h', host_type + " [" + evidence + "]" } });
                    flag_as_open_access_resource = true;
                }
            }

            if (flag_as_open_access_resource and record.getFirstField("OAS") == record.end())
                record.insertField("OAS", { MARC::Subfield('a', "1") });
        }
        marc_writer->write(record);
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    std::unordered_map<std::string, oadoi_info> doi_to_oainfo;
    CreateDoiToUrlMap(argv[1], &doi_to_oainfo);
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[2]));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[3]));
    Augment856(marc_reader.get(), marc_writer.get(), doi_to_oainfo);

    return EXIT_SUCCESS;
}
