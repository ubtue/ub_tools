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
const std::string DOI_KEY("dc.relation.uri");
const std::string PUBLIKATION_TITLE_KEY("dc.title");
const std::string DOI_URL_FORMAT("http://dx.doi.org/");
const std::vector<std::string> NOTIFICATION_EMAIL_ADRESSES = { "andrii.lysohor@uni-tuebingen.de" };


void SendNotificationsForDOI(const std::string &email_subject, const std::string &email_message) {
    if (EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", NOTIFICATION_EMAIL_ADRESSES, email_subject, email_message,
                                      EmailSender::MEDIUM)
        > 299)
        LOG_ERROR("Failed to send the DOI notification email!");
}


void UpdateItem(DbConnection * const &db_writer, const std::string &doi, const std::string &publication_id) {
    db_writer->queryOrDie("UPDATE tuefind_publications SET doi = " + db_writer->escapeAndQuoteString(doi)
                          + ", doi_notification = NOW() WHERE id= " + db_writer->escapeAndQuoteString(publication_id));
}


std::string ParseDataFromXML(std::shared_ptr<const JSON::ArrayNode> top_node_array, const std::string &field) {
    std::string results;
    for (const auto &item_iter : *top_node_array) {
        const std::string key(JSON::LookupString("/key", item_iter, ""));
        const std::string value(JSON::LookupString("/value", item_iter, ""));
        if (field == key) {
            results = value;
        }
    }
    return results;
}


std::string do_replace(const std::string &in, const std::string &from, const std::string &to) {
    return std::regex_replace(in, std::regex(from), to);
}


std::string GetClearDOI(const std::string &doi_link) {
    size_t substring_length = doi_link.find(DOI_URL_FORMAT);
    if (substring_length > 0) {
        return doi_link;
    } else {
        return do_replace(doi_link, DOI_URL_FORMAT, "");
    }
}


bool DownloadAndUpdate(DbConnection * const &db_writer, const std::string &external_document_id, const std::string &publication_id) {
    const std::string DOWNLOAD_URL(DSPACE_SERVER_URL + external_document_id + "/metadata");

    Downloader downloader(DOWNLOAD_URL, Downloader::Params());
    if (downloader.anErrorOccurred()) {
        EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", NOTIFICATION_EMAIL_ADRESSES,
                                      "Error while downloading data from DSpace API",
                                      "Error while downloading data for id : " + downloader.getLastErrorMessage(), EmailSender::MEDIUM);
        return false;
    }

    const HttpHeader http_header(downloader.getMessageHeader());
    if (http_header.getStatusCode() != 200) {
        LOG_WARNING("DOI returned HTTP status code " + std::to_string(http_header.getStatusCode()) + "! for item id: " + publication_id);
        return false;
    }

    const std::string &json_document(downloader.getMessageBody());
    std::shared_ptr<JSON::JSONNode> full_tree;
    JSON::Parser parser(json_document);
    if (not parser.parse(&full_tree))
        LOG_ERROR("failed to parse JSON (" + parser.getErrorMessage());

    const std::shared_ptr<const JSON::ArrayNode> top_node_array(JSON::JSONNode::CastToArrayNodeOrDie("full_tree", full_tree));

    const std::string doi_link = ParseDataFromXML(top_node_array, DOI_KEY);

    if (!doi_link.empty()) {
        const std::string publication_title = ParseDataFromXML(top_node_array, PUBLIKATION_TITLE_KEY);
        const std::string doi = GetClearDOI(doi_link);
        LOG_INFO("Processing: " + doi);
        SendNotificationsForDOI("DOI link notification",
                                "DOI link: " + doi + " successfully generated for publication " + publication_title);
        UpdateItem(db_writer, doi, publication_id);
    }

    return true;
}


} // unnamed namespace


int main() {
    if (not VuFind::GetTueFindFlavour().empty()) {
        auto db_writer(DbConnection::VuFindMySQLFactory());
        auto db_reader((DbConnection::VuFindMySQLFactory()));
        db_reader.queryOrDie("SELECT * FROM tuefind_publications WHERE doi_notification IS NULL");
        auto result_set(db_reader.getLastResultSet());
        DbTransaction transaction(&db_writer);
        while (const auto row = result_set.getNextRow()) {
            DownloadAndUpdate(&db_writer, row["external_document_guid"], row["id"]);
        }
    }

    return 0;
}