#include <iostream>
#include <regex>
#include <set>
#include <unordered_map>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "Downloader.h"
#include "EmailSender.h"
#include "HttpHeader.h"
#include "JSON.h"
#include "StlHelpers.h"
#include "StringUtil.h"
#include "VuFind.h"
#include "util.h"


namespace {


const std::string DSPACE_SERVER_URL("https://publikationen.uni-tuebingen.de/rest/items/");
const std::string DOI_KEY("dc.identifier.uri");
const std::string PUBLICATION_TITLE_KEY("dc.title");
const std::string DOI_URL_PREFIX("http://dx.doi.org/");
const std::vector<std::string> NOTIFICATION_EMAIL_ADRESSES = { "mario.trojan@uni-tuebingen.de" };


void SendNotificationsForDOI(const std::string &email_subject, const std::string &email_message) {
    if (EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", NOTIFICATION_EMAIL_ADRESSES, email_subject, email_message,
                                      EmailSender::MEDIUM)
        > 299)
        LOG_ERROR("Failed to send the DOI notification email!");
}


void UpdateItem(DbConnection * const &db_writer, const std::string &doi, const std::string &publication_id) {
    db_writer->queryOrDie("UPDATE tuefind_publications SET doi = " + db_writer->escapeAndQuoteString(doi)
                          + ", doi_notification_datetime = NOW() WHERE id= " + db_writer->escapeAndQuoteString(publication_id));
}


std::string ParseDataFromJSON(std::shared_ptr<const JSON::ArrayNode> top_node_array, const std::string &field) {
    for (const auto &item_iter : *top_node_array) {
        const std::string key(JSON::LookupString("/key", item_iter, ""));
        const std::string value(JSON::LookupString("/value", item_iter, ""));
        if (field == key)
            return value;
    }

    return "";
}


std::string GetClearDOI(const std::string &doi_url) {
    size_t found(doi_url.find(DOI_URL_PREFIX));
    if (found == std::string::npos)
        return doi_url;
    else
        return StringUtil::ReplaceString(DOI_URL_PREFIX, "", doi_url);
}


void DownloadAndUpdate(DbConnection * const &db_writer, const std::string &external_document_guid, const std::string &publication_id) {
    const std::string DOWNLOAD_URL(DSPACE_SERVER_URL + external_document_guid + "/metadata");

    Downloader::Params params;
    params.additional_headers_.emplace_back("Accept: application/json"); // Default in DSpace 6 would be XML, but we want JSON

    Downloader downloader(DOWNLOAD_URL, params);
    if (downloader.anErrorOccurred()) {
        EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", NOTIFICATION_EMAIL_ADRESSES,
                                      "Error while downloading data from DSpace API",
                                      "Error while downloading data for id : " + downloader.getLastErrorMessage(), EmailSender::MEDIUM);
        return;
    }

    const HttpHeader http_header(downloader.getMessageHeader());
    if (http_header.getStatusCode() != 200) {
        LOG_WARNING("DOI returned HTTP status code " + std::to_string(http_header.getStatusCode()) + "! for item id: " + publication_id);
        return;
    }

    // for sample document with DOI, see:
    // curl -s -H "Accept: application/json"
    // "https://publikationen.uni-tuebingen.de/rest/items/712e63be-0d92-4d8d-aae0-63da7f121797/metadata"
    const std::string &json_document(downloader.getMessageBody());
    std::shared_ptr<JSON::JSONNode> full_tree;
    JSON::Parser parser(json_document);
    if (not parser.parse(&full_tree))
        LOG_ERROR("failed to parse JSON (" + parser.getErrorMessage());

    const std::shared_ptr<const JSON::ArrayNode> top_node_array(JSON::JSONNode::CastToArrayNodeOrDie("full_tree", full_tree));
    const std::string doi_url = ParseDataFromJSON(top_node_array, DOI_KEY);

    if (doi_url.empty()) {
        LOG_INFO("No DOI found yet for item id " + publication_id);
        return;
    }

    const std::string publication_title = ParseDataFromJSON(top_node_array, PUBLICATION_TITLE_KEY);
    const std::string doi = GetClearDOI(doi_url);
    LOG_INFO("Processing: " + doi);
    SendNotificationsForDOI("DOI link notification", "DOI link: " + doi + " successfully generated for publication " + publication_title);
    UpdateItem(db_writer, doi, publication_id);
}


} // unnamed namespace


int main() {
    // Note: It only makes sense to run this program on the live server
    //       because the DSpace test server doesn't register DOIs.

    // We need 2 connections so we can update while iterating a ResultSet.
    auto db_reader((DbConnection::VuFindMySQLFactory()));
    auto db_writer(DbConnection::VuFindMySQLFactory());

    db_reader.queryOrDie("SELECT * FROM tuefind_publications WHERE doi_notification_datetime IS NULL");
    auto result_set(db_reader.getLastResultSet());
    DbTransaction transaction(&db_writer);
    while (const auto row = result_set.getNextRow()) {
        DownloadAndUpdate(&db_writer, row["external_document_guid"], row["id"]);
    }

    return EXIT_SUCCESS;
}
