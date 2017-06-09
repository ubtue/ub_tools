/** \brief IxTheo utility to inform subscribed users of changes in monitored queries etc.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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

/*
A typical config file for this program looks like this:

user     = "root"
passwd   = "???"
database = "vufind"
*/

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <DbConnection.h>
#include <DbResultSet.h>
#include <DbRow.h>
#include <Downloader.h>
#include <GzStream.h>
#include <RegexMatcher.h>
#include <IniFile.h>
#include <StringUtil.h>
#include <UrlUtil.h>
#include <util.h>
#include <XmlParser.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " ini_file_path\n";
    std::exit(EXIT_FAILURE);
}


namespace {


enum class ParseState { ARRAY_EXPECTED, OPEN_PAREN_EXPECTED, PARAM_OR_CLOSE_PAREN_EXPECTED, PARAM_OPEN_PAREN_EXPECTED,
                        PARAM_VALUE_EXPECTED, PARAM_CLOSE_PAREN_EXPECTED };


} // unnamed namespace


/** Parses structures like the following:

Array
(
    [qf] => Array
        (
            [0] => title_short^750 title_full_unstemmed^600 title_full^400 title^500 title_alt^200 title_new^100 series^50 series2^30 author^300 author_fuller^150 contents^10 topic_unstemmed^550 topic^500 geographic^300 genre^300 allfields_unstemmed^10 fulltext_unstemmed^10 allfields fulltext description isbn issn
        )

    [qt] => Array
        (
            [0] => dismax
        )

    [q] => Array
        (
            [0] => brxx
        )
)

This is a pretty-printed PHP array of arrays data type representing a query.
*/
bool ExtractQueryParams(const std::string &php_query_array,
                        std::map<std::string, std::string> * const params_to_values_map,
                        std::string * const err_msg)
{
    err_msg->clear();

    std::vector<std::string> lines;
    if (StringUtil::SplitThenTrim(php_query_array, '\n', "\t ", &lines) <= 1) {
        *err_msg = "Too few lines!";
        return false;
    }

    static RegexMatcher * const param_name_matcher(RegexMatcher::RegexMatcherFactory("\\[([[:lower:]]+)\\]"));
    ParseState parse_state(ParseState::ARRAY_EXPECTED);
    std::string last_parem_name;
    for (std::string line : lines) {
        StringUtil::Trim(&line);
        if (line.empty())
            continue;

        switch (parse_state) {
        case ParseState::ARRAY_EXPECTED:
            if (line != "Array") {
                *err_msg = "\"Array\" expected!";
                return false;
            }
            parse_state = ParseState::OPEN_PAREN_EXPECTED;
            break;
        case ParseState::OPEN_PAREN_EXPECTED:
            if (line != "(") {
                *err_msg = "Open parenthesis expected!";
                return false;
            }
            parse_state = ParseState::PARAM_OR_CLOSE_PAREN_EXPECTED;
            break;
        case ParseState::PARAM_OR_CLOSE_PAREN_EXPECTED:
            if (line == ")") {
                if (params_to_values_map->empty()) {
                    *err_msg = "Closing parenthesis expected!";
                    return false;
                }
                return true;
            }
            if (not param_name_matcher->matched(line)) {
                *err_msg = "line mismatch!";
                return false;
            }
            last_parem_name = (*param_name_matcher)[1];
            parse_state = ParseState::PARAM_OPEN_PAREN_EXPECTED;
            break;
        case ParseState::PARAM_OPEN_PAREN_EXPECTED:
            if (line != "(") {
                *err_msg = "Open parenthesis as part of parameter expression expected!";
                return false;
            }
            parse_state = ParseState::PARAM_VALUE_EXPECTED;
            break;
        case ParseState::PARAM_VALUE_EXPECTED:
            if (not StringUtil::StartsWith(line, "[0] => ")) {
                *err_msg = "line did not start with \"[0] => \"!";
                return false;
            }
            (*params_to_values_map)[last_parem_name] = line.substr(7);
            parse_state = ParseState::PARAM_CLOSE_PAREN_EXPECTED;
            break;
        case ParseState::PARAM_CLOSE_PAREN_EXPECTED:
            if (line != ")") {
                *err_msg = "Closing parenthesis as part of parameter expression expected!";
                return false;
            }
            parse_state = ParseState::PARAM_OR_CLOSE_PAREN_EXPECTED;
            break;
        }
    }

    *err_msg = "We should *never* get here!";
    return false;
}


// Contacts VuFind to get the SOLR query parameters given a serialised minSO PHP object.
void GetQueryParams(const std::string &serialised_minSO,
                    std::map<std::string, std::string> * const params_to_values_map)
{
    params_to_values_map->clear();

    static const std::string URL_BASE("http://localhost/Devtools/Deminify?min=");
    static const unsigned TIMEOUT(10); // seconds
    std::string web_document;
    if (Download(URL_BASE + UrlUtil::UrlEncode(serialised_minSO), TIMEOUT, &web_document) != 0)
        throw std::runtime_error("Failed to contact VuFind w/in " + std::to_string(TIMEOUT) + " seconds!");

    const std::string::size_type pre_start_pos(web_document.find("<pre>"));
    if (pre_start_pos == std::string::npos)
        throw std::runtime_error("Failed to find <pre>!");

    const std::string::size_type pre_end_pos(web_document.find("</pre>"));
    if (pre_end_pos == std::string::npos)
        throw std::runtime_error("Failed to find </pre>!");

    std::string err_msg;
    if (not ExtractQueryParams(web_document.substr(pre_start_pos + 5, pre_end_pos - pre_start_pos - 5),
                               params_to_values_map, &err_msg))
        throw std::runtime_error("Failed to extract query parameters: " + err_msg);
}


std::string GenerateSolrQuery(const std::map<std::string, std::string> &params_to_values_map) {
    std::string url("http://localhost:8080/solr/biblio/select?");
    for (const auto &key_and_value : params_to_values_map) {
        if (url[url.length() - 1] != '?')
            url += '&';
        url += key_and_value.first + "=" + UrlUtil::UrlEncode(key_and_value.second);
    }
    url += "&fl=id"; // We only need ID's anyway.
    url += "&rows=10000"; // Let's hope that no user is interested in more than the first 10k documents.

    return url;
}


/** \brief Extracts ID's between <str name="id"> and </str> tags. */
class IdExtractor: public XmlParser {
    std::string first_error_;
    std::vector<std::string> extracted_ids_;
    bool between_opening_and_closing_id_str_tags_;
    std::string current_id_;
public:
    explicit IdExtractor(const std::string &document)
        : XmlParser(document.c_str(), document.size(), /* convert_to_iso8859_15 = */false,
                    XmlParser::START_ELEMENT | XmlParser::END_ELEMENT | XmlParser::CHARACTERS
                    | XmlParser::ERROR | XmlParser::FATAL_ERROR),
          between_opening_and_closing_id_str_tags_(false) { }
    virtual void notify(const Chunk &chunk);
    const std::string &getFirstError() const { return first_error_; }
    void getExtractedIds(std::vector<std::string> * const extracted_ids) {
        extracted_ids->swap(extracted_ids_);
    }
};


void IdExtractor::notify(const Chunk &chunk) {
    if (not first_error_.empty())
        return;

    if (chunk.type_ == XmlParser::START_ELEMENT) {
        if (chunk.text_ != "str" or chunk.attribute_map_->find("name") == chunk.attribute_map_->end())
            return;
        if ((*chunk.attribute_map_)["name"] != "id")
            return;
        between_opening_and_closing_id_str_tags_ = true;
    } else if (chunk.type_ == XmlParser::CHARACTERS) {
        if (between_opening_and_closing_id_str_tags_)
            current_id_ += chunk.text_;
    } else if (chunk.type_ == XmlParser::END_ELEMENT) {
        if (between_opening_and_closing_id_str_tags_) {
            between_opening_and_closing_id_str_tags_ = false;
            extracted_ids_.emplace_back(current_id_);
            current_id_.clear();
        }
    } else if (chunk.type_ == XmlParser::ERROR)
        first_error_ = chunk.text_;
    else if (chunk.type_ == XmlParser::FATAL_ERROR)
        first_error_ = chunk.text_;
}


/** \brief Given two sorted vectors of ID's, extract the ID's which are only in "new_ids". */
void FindNewIds(const std::vector<std::string> &old_ids, const std::vector<std::string> &new_ids,
                std::vector<std::string> * const additional_ids)
{
    additional_ids->clear();

    std::vector<std::string>::const_iterator old_id(old_ids.begin());
    std::vector<std::string>::const_iterator new_id(new_ids.begin());

    while (old_id != old_ids.end()) {
        if (new_id == new_ids.end())
            return;

        if (*old_id == *new_id)
            ++old_id, ++new_id;
        else if (*old_id < *new_id)
            ++old_id;
        else if (*old_id > *new_id) {
            additional_ids->emplace_back(*new_id);
            ++new_id;
        }
    }

    while (new_id != new_ids.end())
        additional_ids->emplace_back(*new_id++);
}


// Turn a vector of ID's into a compressed colon-separted string of ID's.
void SerialiseIds(const std::vector<std::string> &ids, std::string * const serialized_ids) {
    std::string uncompressed_string;
    uncompressed_string.reserve(ids.size() * (9 + 1)/* PPN length + colon */);

    for (const auto &id : ids)
        uncompressed_string += id + ":";
    if (not uncompressed_string.empty())
        uncompressed_string.resize(uncompressed_string.length() - 1); // Remove the trailing colon.

    *serialized_ids = GzStream::CompressString(uncompressed_string);
}


// Turn a compressed colon-separted string of ID's into a vector of ID's.
void DeserialiseIds(const std::string &serialized_ids, std::vector<std::string> * const deserialised_ids) {
    deserialised_ids->clear();

    const std::string decompressed_string(GzStream::DecompressString(serialized_ids));
    if (unlikely(decompressed_string.empty()))
        return;

    std::string::size_type id_start_pos(0);
    std::string::size_type colon_pos(decompressed_string.find(':'));
    while (colon_pos != std::string::npos) {
        deserialised_ids->emplace_back(decompressed_string.substr(id_start_pos, colon_pos - id_start_pos));
        id_start_pos = colon_pos + 1;
        colon_pos = decompressed_string.find(':', id_start_pos);
    }

    deserialised_ids->emplace_back(decompressed_string.c_str() + colon_pos + 1);
}


void InsertIdsIntoTheIxtheoIdResultSetsTable(const std::string &query_id, const std::vector<std::string> &ids,
                                             DbConnection * const connection)
{
    std::string serialized_ids;
    SerialiseIds(ids, &serialized_ids);
    connection->queryOrDie("REPLACE INTO ixtheo_id_result_sets (id,ids) VALUES(" + query_id + ",\""
                           + connection->escapeString(serialized_ids) + "\")");
}


/** \return true = we need to notify the user that something has changed that they would like to know about */
bool ProcessUser(const std::string &user_id, const std::string &/*email_address*/, DbConnection * const connection) {
    connection->queryOrDie("SELECT id,search_object FROM search WHERE user_id=" + user_id);

    constexpr unsigned SOLR_QUERY_TIMEOUT(20); // seconds
    DbResultSet search_object_result_set(connection->getLastResultSet());
    while (const DbRow row = search_object_result_set.getNextRow()) {
        const std::string query_id(row[0]);
        const std::string serialised_search_object(row[1]);

        std::map<std::string, std::string> params_to_values_map;
        GetQueryParams(serialised_search_object, &params_to_values_map);
        const std::string solr_query_url(GenerateSolrQuery(params_to_values_map));

        std::string xml_document;
        if (Download(solr_query_url, SOLR_QUERY_TIMEOUT, &xml_document) != 0) {
            Warning("SOLR query failed! (" + solr_query_url + ")");
            return false;
        }

        IdExtractor id_extractor(xml_document);
        if (not id_extractor.parse()) {
            Warning("Failed to parse XML document! (" + id_extractor.getFirstError() + ")");
            return false;
        }

        std::vector<std::string> ids;
        id_extractor.getExtractedIds(&ids);
        std::sort(ids.begin(), ids.end());

        connection->queryOrDie("SELECT ids FROM ixtheo_id_result_sets WHERE id=" + query_id);
        DbResultSet ids_result_set(connection->getLastResultSet());

        if (ids_result_set.empty()) // We have nothing to compare against this time.
            InsertIdsIntoTheIxtheoIdResultSetsTable(query_id, ids, connection);
        else { // We need to compare against the previously stored list of ID's.
            const DbRow serialised_ids_row(ids_result_set.getNextRow());
            const std::string serialised_ids(serialised_ids_row[0]);
            std::vector<std::string> old_ids;
            DeserialiseIds(serialised_ids, &old_ids);

            std::vector<std::string> additional_ids;
            FindNewIds(old_ids, ids, &additional_ids);
            if (not additional_ids.empty()) {
                InsertIdsIntoTheIxtheoIdResultSetsTable(query_id, ids, connection);
            }
        }
    }

    return true;
}


struct UserIdAndEmail {
    const std::string user_id_;
    const std::string email_;
public:
    UserIdAndEmail(const std::string &user_id, const std::string &email): user_id_(user_id), email_(email) { }
};


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    try {
        const IniFile ini_file(argv[1]);
        const std::string user(ini_file.getString("", "user"));
        const std::string passwd(ini_file.getString("", "passwd"));
        const std::string db(ini_file.getString("", "database"));

        DbConnection connection(db, user, passwd);
        connection.queryOrDie("SELECT id,email FROM user");
        DbResultSet result_set(connection.getLastResultSet());
        DbRow row;
        while (row = result_set.getNextRow()) {
            const std::string id(row[0]);
            const std::string email_address(row[1]);
            if (not ProcessUser(id, email_address, &connection))
                Error("Failed to process user w/ ID: " + id);
        }
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
