/** \brief Utility for replacing old BSZ PPN's with new K10+ PPN's.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "JSON.h"
#include "MARC.h"
#include "Solr.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("solr_host_and_port1 [solr_host_and_port2 .. solr_host_and_portN] marc_input marc_output field_and_subfield_code1 "
            "[field_and_subfield_code2 .. field_and_subfield_codeN]\n"
            "The syntax for hosts and ports is hostname:portnumber.\n"
            "For field_and_subfield_code an example would be 773w.");
}


void PopulateMap(const std::string &solr_host_and_port, std::unordered_map<std::string, std::string> * const old_ppns_to_new_ppns_map) {
    const std::string QUERY("oldid:[* TO *]");
    std::string json_result, err_msg;
    if (unlikely(not Solr::Query(QUERY, "id,old_id", &json_result, &err_msg, solr_host_and_port, /* timeout = */ 5, Solr::JSON)))
        LOG_ERROR("Solr query failed or timed-out: \"" + QUERY + "\". (" + err_msg + ")");

    JSON::Parser parser(json_result);
    std::shared_ptr<JSON::JSONNode> tree;
    if (not parser.parse(&tree))
        LOG_ERROR("JSON parser failed: " + parser.getErrorMessage());

    const std::shared_ptr<const JSON::ObjectNode> tree_obj(JSON::JSONNode::CastToObjectNodeOrDie("top level JSON entity", tree));
    const std::shared_ptr<const JSON::ObjectNode> response(tree_obj->getObjectNode("response"));
    const std::shared_ptr<const JSON::ArrayNode> docs(response->getArrayNode("docs"));

    for (const auto &doc : *docs) {
        const std::shared_ptr<const JSON::ObjectNode> doc_obj(JSON::JSONNode::CastToObjectNodeOrDie("document object", doc));

        const std::string id(JSON::LookupString("/id", doc_obj, /* default_value = */""));
        if (unlikely(id.empty()))
            LOG_ERROR("Did not find 'id' node in JSON tree!");

        const std::string oldid(JSON::LookupString("/oldid", doc_obj, /* default_value = */""));
        if (unlikely(oldid.empty()))
            LOG_ERROR("Did not find 'oldid' node in JSON tree!");

        old_ppns_to_new_ppns_map->emplace(id, oldid);
    }

    LOG_INFO("Found " + std::to_string(old_ppns_to_new_ppns_map->size()) + " mappings from old PPN's to new PPN's in Solr. ("
             + solr_host_and_port + ")");
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    const std::vector<std::string> &tags_and_subfield_codes,
                    const std::unordered_map<std::string, std::string> &old_ppns_to_new_ppns_map)
{
    unsigned total_record_count(0), patched_record_count(0);
    while (MARC::Record record = marc_reader->read()) {
            ++total_record_count;

            bool patched_record(false);
            for (const auto tag_and_subfield_code : tags_and_subfield_codes) {
                for (auto field : record.getTagRange(tag_and_subfield_code.substr(0, MARC::Record::TAG_LENGTH))) {
                    const char SUBFIELD_CODE(tag_and_subfield_code[MARC::Record::TAG_LENGTH]);
                    MARC::Subfields subfields(field.getSubfields());
                    bool patch_field(false);
                    for (auto &subfield : subfields) {
                        if (subfield.code_ != SUBFIELD_CODE)
                            continue;

                        std::string old_ppn_candidate;
                        if (StringUtil::StartsWith(subfield.value_, "(DE-576)"))
                            old_ppn_candidate = subfield.value_.substr(__builtin_strlen("(DE-576)"));
                        else
                            old_ppn_candidate = subfield.value_;

                        const auto old_ppn_and_new_ppn(old_ppns_to_new_ppns_map.find(old_ppn_candidate));
                        if (old_ppn_and_new_ppn == old_ppns_to_new_ppns_map.cend())
                            continue;

                        subfield.value_ = old_ppn_and_new_ppn->second;
                        patch_field = true;
                    }

                    if (patch_field) {
                        field.setContents(subfields, field.getIndicator1(), field.getIndicator2());
                        patched_record = true;
                    }
                }
            }
            if (patched_record)
                ++patched_record_count;

            marc_writer->write(record);
    }

    LOG_INFO("Processed " + std::to_string(total_record_count) + " records and patched " + std::to_string(patched_record_count)
             + " of them.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        Usage();

    std::vector<std::string> solr_hosts_and_ports;
    while (argc > 1 and std::strchr(argv[1], ':') != nullptr) {
        solr_hosts_and_ports.emplace_back(argv[1]);
        --argc, ++argv;
    }
    if (solr_hosts_and_ports.empty())
        LOG_ERROR("we need at one Solr hostname and port number!");

    if (argc < 4)
        Usage();

    std::vector<std::string> tags_and_subfield_codes;
    for (int arg_no(3); arg_no < argc; ++arg_no) {
        if (std::strlen(argv[arg_no]) != MARC::Record::TAG_LENGTH + 1)
            LOG_ERROR("bad tag + subfield code: \"" + std::string(argv[arg_no]) + "\"!");
        tags_and_subfield_codes.emplace_back(argv[arg_no]);
    }
    std::sort(tags_and_subfield_codes.begin(), tags_and_subfield_codes.end());

    std::unordered_map<std::string, std::string> old_ppns_to_new_ppns_map;
    for (const auto &solr_host_and_port : solr_hosts_and_ports)
        PopulateMap(solr_host_and_port, &old_ppns_to_new_ppns_map);

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto marc_writer(MARC::Writer::Factory(argv[2]));
    ProcessRecords(marc_reader.get(), marc_writer.get(), tags_and_subfield_codes, old_ppns_to_new_ppns_map);

    return EXIT_SUCCESS;
}
