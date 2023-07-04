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
const std::string DOI_URL_PREFIX("http://dx.doi.org/");
const std::vector<std::string> NOTIFICATION_EMAIL_ADRESSES = { "mario.trojan@uni-tuebingen.de" };


struct DSpaceItem {
    std::string author_;
    std::string doi_;
    std::string title_;

    DSpaceItem(const std::string &json) {
        std::shared_ptr<JSON::JSONNode> full_tree;
        JSON::Parser parser(json);
        if (not parser.parse(&full_tree))
            LOG_ERROR("failed to parse JSON (" + parser.getErrorMessage());

        const std::shared_ptr<const JSON::ArrayNode> top_node_array(JSON::JSONNode::CastToArrayNodeOrDie("full_tree", full_tree));
        for (const auto &item_iter : *top_node_array) {
            const std::string key(JSON::LookupString("/key", item_iter, ""));
            const std::string value(JSON::LookupString("/value", item_iter, ""));
            if (key == "dc.title")
                title_ = value;
            else if (key == "dc.contributor.author")
                author_ = value;
            else if (key == "dc.identifier.uri") {
                if (StringUtil::StartsWith(value, DOI_URL_PREFIX)) {
                    doi_ = StringUtil::ReplaceString(DOI_URL_PREFIX, "", value);
                }
            }
        }
    }
};


void SendNotificationsForItem(const DSpaceItem &dspace_item) {
    const std::string subject(dspace_item.author_);
    const std::string body(dspace_item.title_ + "\n" + DOI_URL_PREFIX + dspace_item.doi_);

    if (EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", NOTIFICATION_EMAIL_ADRESSES, subject, body, EmailSender::MEDIUM)
        > 299)
        LOG_ERROR("Failed to send the DOI notification email!");
}


void UpdateItem(DbConnection * const &db_writer, const std::string &doi, const std::string &publication_id) {
    db_writer->queryOrDie("UPDATE tuefind_publications SET doi = " + db_writer->escapeAndQuoteString(doi)
                          + ", doi_notification_datetime = NOW() WHERE id=" + db_writer->escapeAndQuoteString(publication_id));
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
    const std::string &json(downloader.getMessageBody());
    const DSpaceItem dspace_item(json);

    if (dspace_item.doi_.empty()) {
        LOG_INFO("No DOI found yet for item id " + publication_id);
        return;
    }

    LOG_INFO("Processing: " + dspace_item.doi_);
    SendNotificationsForItem(dspace_item);
    UpdateItem(db_writer, dspace_item.doi_, publication_id);
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
    while (const auto row = result_set.getNextRow()) {
        DownloadAndUpdate(&db_writer, row["external_document_guid"], row["id"]);
    }

    return EXIT_SUCCESS;
}
