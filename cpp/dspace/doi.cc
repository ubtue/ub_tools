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

using namespace std;

const std::string DspaceServerURL("https://publikationen.uni-tuebingen.de/rest/items/");
const std::string DOIKey("dc.relation.uri");
const std::string PublicationTitleKey("dc.title");
const std::vector<std::string> notification_email_addresses = { "andrii.lysohor@uni-tuebingen.de" };

void SendNotificationsForDOI(const std::string email_subject, const std::string email_message) {
    if (EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", notification_email_addresses, email_subject, email_message,
                                      EmailSender::VERY_HIGH)
        > 299)
        LOG_ERROR("Failed to send the DOI notification email!");
}

void UpdateItem(DbConnection * const db_writer, const std::string doi_link, const std::string publication_id) {
    db_writer->queryOrDie("UPDATE tuefind_publications SET doi_link = " + db_writer->escapeAndQuoteString(doi_link)
                          + ",doi_notification = CURDATE() WHERE id= " + publication_id + "");
}

bool DownloadAndUpdate(DbConnection * const db_writer, const std::string external_document_id, const std::string publicationID) {
    const std::string DOWNLOAD_URL(DspaceServerURL + external_document_id + "/metadata");

    Downloader downloader(DOWNLOAD_URL, Downloader::Params(), 15 * 1000);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Error while downloading data for id : " + downloader.getLastErrorMessage());
        return false;
    }

    const HttpHeader http_header(downloader.getMessageHeader());
    if (http_header.getStatusCode() != 200) {
        LOG_WARNING("DOI returned HTTP status code " + std::to_string(http_header.getStatusCode()) + "! for item id: " + publicationID);
        return false;
    }

    const std::string &json_document(downloader.getMessageBody());
    std::shared_ptr<JSON::JSONNode> full_tree;
    JSON::Parser parser(json_document);
    if (not parser.parse(&full_tree))
        LOG_ERROR("failed to parse JSON (" + parser.getErrorMessage());

    const std::shared_ptr<const JSON::ArrayNode> top_node_array(JSON::JSONNode::CastToArrayNodeOrDie("full_tree", full_tree));

    std::string PublicationTitle;

    for (auto item_iter : *top_node_array) {
        const std::string title_key(JSON::LookupString("/key", item_iter, ""));
        const std::string title_value(JSON::LookupString("/value", item_iter, ""));
        if (PublicationTitleKey == title_key) {
            PublicationTitle = title_value;
        }
    }

    for (auto item_iter : *top_node_array) {
        const std::string one_key(JSON::LookupString("/key", item_iter, ""));
        const std::string DOILink(JSON::LookupString("/value", item_iter, ""));
        if (DOIKey == one_key) {
            std::cout << "Processing: " << DOILink << '\n';
            UpdateItem(db_writer, DOILink, publicationID);
            SendNotificationsForDOI("DOI link notification",
                                    "DOI link: " + DOILink + " successfully generated for publication " + PublicationTitle);
        }
    }

    return true;
}

int main() {
    auto db_reader((DbConnection::VuFindMySQLFactory()));

    if (not VuFind::GetTueFindFlavour().empty()) {
        auto db_writer(DbConnection::VuFindMySQLFactory());
        db_reader.queryOrDie("SELECT * FROM tuefind_publications WHERE doi_link IS NULL OR doi_link=''");
        auto result_set(db_reader.getLastResultSet());
        DbTransaction transaction(&db_writer);
        while (const auto row = result_set.getNextRow()) {
            DownloadAndUpdate(&db_writer, row["external_document_guid"], row["id"]);
        }
    }

    return 0;
}