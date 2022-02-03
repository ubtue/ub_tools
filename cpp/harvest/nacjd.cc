/** \file    nacjd.cc
    \brief   Identifies URLs that we can use for further processing.
    \author  andreas-ub

    \copyright 2021 Universitätsbibliothek Tübingen

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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <cerrno>
#include <cstring>
#include <getopt.h>
#include "Downloader.h"
#include "FileUtil.h"
#include "HttpHeader.h"
#include "JSON.h"
#include "MARC.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "marc_title_in_file new_marc_title_out_file\n"
        "where marc_title_in_file contains also icpsr records (001 or 035a)\n"
        "these records are not processed any more\n"
        "new_marc_title_out_file contains all icpsr records not contained in input file.");
}


std::vector<std::string> ids_website;
const unsigned int TIMEOUT_IN_SECONDS(15);
const std::string NACJD_TITLES("/tmp/nacjd_titles.html");
const std::string NACJD_NEW_TITLES_JSON("/tmp/nacjd_new_titles.json");


bool ContainsValue(const std::map<std::string, std::string> &map, const std::string &search_value) {
    for (const auto &[key, val] : map) {
        if (val == search_value)
            return true;
    }
    return false;
}


enum PreviousChar { ID_START, FIRST_DOUBLE_QUOTE_SEEN, CAP_I_SEEN, CAP_D_SEEN, SECOND_DOUBLE_QUOTE_SEEN, COLON_SEEN };


void HandleChar(const char c) {
    static std::string current_id_website;
    static PreviousChar state(ID_START);
    if (state == ID_START) {
        if (c == '"')
            state = FIRST_DOUBLE_QUOTE_SEEN;
    } else if (state == FIRST_DOUBLE_QUOTE_SEEN) {
        if (c == 'I')
            state = CAP_I_SEEN;
        else
            state = ID_START;
    } else if (state == CAP_I_SEEN) {
        if (c == 'D')
            state = CAP_D_SEEN;
        else
            state = ID_START;
    } else if (state == CAP_D_SEEN) {
        if (c == '"')
            state = SECOND_DOUBLE_QUOTE_SEEN;
        else
            state = ID_START;
    } else if (state == SECOND_DOUBLE_QUOTE_SEEN) {
        if (c == ':')
            state = COLON_SEEN;
        else
            state = ID_START;
    } else if (state == COLON_SEEN) {
        if (c == ',') {
            ids_website.push_back(current_id_website);
            current_id_website.clear();
            state = ID_START;
        } else
            current_id_website += c;
    }
}


bool DownloadID(std::ofstream &json_new_titles, const std::string &id, const bool use_separator) {
    const std::string DOWNLOAD_URL("https://pcms.icpsr.umich.edu/pcms/api/1.0/studies/" + id
                                   + "/dats?page=https://www.icpsr.umich.edu/web/NACJD/studies/" + id + "/export&user=");

    Downloader downloader(DOWNLOAD_URL, Downloader::Params(), TIMEOUT_IN_SECONDS * 1000);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Error while downloading data for id " + id + ": " + downloader.getLastErrorMessage());
        return false;
    }

    // Check for rate limiting and error status codes:
    const HttpHeader http_header(downloader.getMessageHeader());
    if (http_header.getStatusCode() != 200) {
        LOG_WARNING("NACJD returned HTTP status code " + std::to_string(http_header.getStatusCode()) + "! for NACJD id: " + id);
        return false;
    }

    const std::string &json_document(downloader.getMessageBody());
    std::shared_ptr<JSON::JSONNode> full_tree;
    JSON::Parser parser(json_document);
    if (not parser.parse(&full_tree)) {
        LOG_ERROR("failed to parse JSON (" + parser.getErrorMessage() + "), download URL was: " + DOWNLOAD_URL);
        return false;
    }

    const std::shared_ptr<const JSON::ObjectNode> top_node(JSON::JSONNode::CastToObjectNodeOrDie("full_tree", full_tree));
    if (use_separator)
        json_new_titles << ',' << '\n';
    json_new_titles << top_node->toString() << '\n';

    return true;
}


void ExtractExistingIDsFromMarc(MARC::Reader * const marc_reader, std::set<std::string> * const parsed_marc_ids) {
    while (MARC::Record record = marc_reader->read()) {
        std::string ppn(record.getControlNumber());
        if (StringUtil::Contains(ppn, "[ICPSR]")) {
            StringUtil::ReplaceString("[ICPSR]", "", &ppn);
            StringUtil::TrimWhite(&ppn);
            parsed_marc_ids->emplace(ppn);
        }

        std::string id_035(record.getFirstSubfieldValue("035", 'a'));
        if (StringUtil::Contains(id_035, "[ICPSR]")) {
            StringUtil::ReplaceString("[ICPSR]", "", &id_035);
            StringUtil::TrimWhite(&id_035);
            parsed_marc_ids->emplace(id_035);
        }
    }
}


void ExtractIDsFromWebsite(const std::set<std::string> &parsed_marc_ids, unsigned * const number_of_new_ids) {
    const std::string DOWNLOAD_URL(
        "https://www.icpsr.umich.edu/web/NACJD/search/"
        "studies?start=0&ARCHIVE=NACJD&PUBLISH_STATUS=PUBLISHED&sort=DATEUPDATED%20desc&rows=9000");
    if (FileUtil::Exists(NACJD_TITLES))
        FileUtil::DeleteFile(NACJD_TITLES);

    if (not Download(DOWNLOAD_URL, NACJD_TITLES, TIMEOUT_IN_SECONDS * 1000))
        LOG_ERROR("Could not download website with nacjd ids.");
    std::ifstream file(NACJD_TITLES);
    if (file)
        std::for_each(std::istream_iterator<char>(file), std::istream_iterator<char>(), HandleChar);
    else
        LOG_ERROR("couldn't open file: " + NACJD_TITLES);

    if (FileUtil::Exists(NACJD_NEW_TITLES_JSON) and not FileUtil::DeleteFile(NACJD_NEW_TITLES_JSON))
        LOG_ERROR("Could not delete file: " + NACJD_NEW_TITLES_JSON);
    std::ofstream json_new_titles(NACJD_NEW_TITLES_JSON);
    json_new_titles << "{ \"nacjd\" : [ " << '\n';
    bool first(true);
    for (const auto &id : ids_website) {
        if (parsed_marc_ids.find(id) != parsed_marc_ids.end())
            continue;
        const bool success(DownloadID(json_new_titles, id, /*use_separator*/ not first));
        if (first and success)
            first = false;
        if (success)
            ++(*number_of_new_ids);
    }
    json_new_titles << " ] }";
}


void ParseJSONAndWriteMARC(MARC::Writer * const title_writer) {
    std::string json_document;
    FileUtil::ReadStringOrDie(NACJD_NEW_TITLES_JSON, &json_document);
    JSON::Parser json_parser(json_document);
    std::shared_ptr<JSON::JSONNode> internal_tree_root;
    if (not json_parser.parse(&internal_tree_root))
        LOG_ERROR("Could not properly parse \"" + NACJD_NEW_TITLES_JSON + ": " + json_parser.getErrorMessage());

    const auto root_node(JSON::JSONNode::CastToObjectNodeOrDie("tree_root", internal_tree_root));
    unsigned no_total(0), no_title(0), no_description(0), no_license(0), no_initial_date(0), no_keywords(0), no_creators(0);

    std::shared_ptr<JSON::ArrayNode> nacjd_nodes(JSON::JSONNode::CastToArrayNodeOrDie("nacjd", root_node->getNode("nacjd")));
    for (const auto &internal_nacjd_node : *nacjd_nodes) {
        ++no_total;
        bool complete(true);
        std::string description;
        std::string license;
        std::string initial_release_date;
        std::set<std::string> keywords;
        std::map<std::string, std::string> creators;
        const auto nacjd_node(JSON::JSONNode::CastToObjectNodeOrDie("entry", internal_nacjd_node));
        const auto alternate_identifiers_node(nacjd_node->getArrayNode("alternateIdentifiers"));
        const auto distributions_node(nacjd_node->getArrayNode("distributions"));
        const auto keywords_node(nacjd_node->getOptionalArrayNode("keywords"));
        const auto creators_node(nacjd_node->getArrayNode("creators"));
        const auto description_node(nacjd_node->getOptionalStringNode("description"));
        const auto title_node(nacjd_node->getStringNode("title"));
        if (description_node == nullptr)
            complete = false;
        else {
            description = description_node->getValue();
            if (description.empty()) {
                complete = false;
                ++no_description;
            } else if (description.length() > MARC::Record::MAX_VARIABLE_FIELD_DATA_LENGTH) {
                const unsigned REDUCE_LENGTH_CHARS(7);
                StringUtil::Truncate(MARC::Record::MAX_VARIABLE_FIELD_DATA_LENGTH - REDUCE_LENGTH_CHARS, &description);
                description += "...";
            }
        }

        for (const auto &internal_distribution_node : *distributions_node) {
            const auto distribution_node(JSON::JSONNode::CastToObjectNodeOrDie("distribution", internal_distribution_node));
            const auto dates_node(distribution_node->getArrayNode("dates"));
            const auto licenses_node(distribution_node->getArrayNode("licenses"));

            for (const auto &internal_date_node : *dates_node) {
                const auto date_node(JSON::JSONNode::CastToObjectNodeOrDie("date", internal_date_node));
                const auto date_type_node(date_node->getObjectNode("type"));
                const auto date_date_node(date_node->getStringNode("date"));
                if (date_type_node->getStringNode("value")->getValue() == "initial release date")
                    initial_release_date = date_date_node->getValue();
            }

            for (const auto &internal_license_node : *licenses_node) {
                const auto license_node(JSON::JSONNode::CastToObjectNodeOrDie("license", internal_license_node));
                const auto license_name_node(license_node->getStringNode("name"));
                if (not license_name_node->getValue().empty()) {
                    license = license_name_node->getValue();
                    break;
                }
            }
            if (not license.empty() and not initial_release_date.empty())
                break;
        }

        if (keywords_node != nullptr) {
            for (const auto &internal_keyword_node : *keywords_node) {
                const auto keyword_node(JSON::JSONNode::CastToObjectNodeOrDie("keyword", internal_keyword_node));
                const auto keyword_value_node(keyword_node->getStringNode("value"));
                if (not keyword_value_node->getValue().empty())
                    keywords.emplace(keyword_value_node->getValue());
            }
        } else
            ++no_keywords;

        for (const auto &internal_creator_node : *creators_node) {
            const auto creator_node(JSON::JSONNode::CastToObjectNodeOrDie("creator", internal_creator_node));
            const std::string type(creator_node->getStringNode("@type")->getValue());
            if (type == "Organization") {
                const std::string name(creator_node->getStringNode("name")->getValue());
                if (name.empty())
                    continue;
                if (ContainsValue(creators, "110"))
                    creators.emplace(name, "710");
                else
                    creators.emplace(name, "110");
            } else if (type == "Person") {
                const auto fullName_node(creator_node->getOptionalStringNode("fullName"));
                if (fullName_node == nullptr)
                    continue;
                std::string fullName = fullName_node->getValue();
                if (fullName.empty())
                    continue;
                if (ContainsValue(creators, "100"))
                    creators.emplace(fullName, "700");
                else
                    creators.emplace(fullName, "100");
            } else
                LOG_ERROR("unknown creator type: " + type);
        }

        if (creators.empty() or license.empty() or initial_release_date.empty()) {
            complete = false;
            if (creators.empty())
                ++no_creators;
            if (license.empty())
                ++no_license;
            if (initial_release_date.empty())
                ++no_initial_date;
        }

        for (const auto &internal_alternateIdentifier : *alternate_identifiers_node) {
            const auto alternateIdentifier_node(JSON::JSONNode::CastToObjectNodeOrDie("alternateIdentifier", internal_alternateIdentifier));
            const std::string id(alternateIdentifier_node->getStringNode("identifier")->getValue());
            if (complete) {
                MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::UNDEFINED,
                                        "[ICPSR]" + id);
                new_record.insertField("245", { { 'a', title_node->getValue() } }, /* indicator 1 = */ '0', /* indicator 2 = */ '0');
                new_record.insertField("520", { { 'a', description } });
                new_record.insertField("540", { { 'a', license } });
                new_record.insertField("264", { { 'c', initial_release_date } });
                new_record.insertField("856", { { 'u', "https://www.icpsr.umich.edu/web/NACJD/studies/" + id } });
                for (auto creator : creators)
                    new_record.insertField(creator.second, { { 'a', creator.first } });
                for (const auto &keyword : keywords) {
                    const std::string normalized_keyword(TextUtil::CollapseAndTrimWhitespace(keyword));
                    new_record.insertField(MARC::GetIndexField(normalized_keyword));
                }
                title_writer->write(new_record);
                break;
            }
        }
    }
    LOG_INFO("Processed: " + std::to_string(no_total) + "entries. " + std::to_string(no_initial_date) + " w/o initial date, "
             + std::to_string(no_title) + " w/o title, " + std::to_string(no_creators) + " w/o creator and " + std::to_string(no_license)
             + " w/o license.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));

    // parse marc_file (later phase_x) and store ids in set
    std::set<std::string> parsed_marc_ids;
    LOG_INFO("Extracting existing ICPSR ids from marc input...");
    ExtractExistingIDsFromMarc(marc_reader.get(), &parsed_marc_ids);
    LOG_INFO("Found: " + std::to_string(parsed_marc_ids.size()) + " records with ICPSR ids.");

    // Download possible new publications and store in json file
    LOG_INFO("Extracting ICPSR ids from website...");
    unsigned number_of_new_ids;
    ExtractIDsFromWebsite(parsed_marc_ids, &number_of_new_ids);
    LOG_INFO(std::to_string(number_of_new_ids) + " new ids collected from website.");

    // parse json file and store relevant information in variables
    // write marc_records via marc_writer to marc_file
    LOG_INFO("Parsing intermediate json file and save to marc output...");
    ParseJSONAndWriteMARC(marc_writer.get());
    LOG_INFO("Finished.");

    return EXIT_SUCCESS;
}
