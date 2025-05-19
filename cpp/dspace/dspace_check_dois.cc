/** \brief A tool for quering a DSpace server to detect registered DOIs for
 *         already known + published items.
 *  \author Andrii Lysohor (andrii.lysohor@uni-tuebingen.de)
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2022,2023 Universitätsbibliothek Tübingen.  All rights reserved.
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


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " dspace_server_url notification_email_addresses\n";
    std::cerr << "       dspace_server_url (w/ trailing slash) e.g.: \"https://publikationen.uni-tuebingen.de/\"\n";
    std::cerr
        << "       notification_email_addresses: semicolon-separated, e.g. \"ixtheo-team@ub.uni-tuebingen.de;...@in.meistertask.com\"\n";
    std::exit(EXIT_FAILURE);
}


const std::string DOI_URL_PREFIX("http://dx.doi.org/");
const std::string EMAIL_SENDER("no-reply@ub.uni-tuebingen.de");


struct DSpaceItem {
    std::vector<std::string> authors_;
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
                authors_.emplace_back(value);
            else if (key == "dc.identifier.uri") {
                if (StringUtil::StartsWith(value, DOI_URL_PREFIX)) {
                    doi_ = StringUtil::ReplaceString(DOI_URL_PREFIX, "", value);
                }
            }
        }
    }
};


void SendNotificationsForItem(const DSpaceItem &dspace_item, const std::vector<std::string> &notification_mail_addresses) {
    std::string subject("Neue Zweitveröffentlichung");
    if (dspace_item.authors_.size() > 0)
        subject = StringUtil::Join(dspace_item.authors_, ", ");
    const std::string body(dspace_item.title_ + "\n" + DOI_URL_PREFIX + dspace_item.doi_);

    if (EmailSender::SimplerSendEmail(EMAIL_SENDER, notification_mail_addresses, subject, body, EmailSender::VERY_HIGH) > 299)
        LOG_ERROR("Failed to send the DOI notification email!");
}


void UpdateItem(DbConnection * const &db_writer, const std::string &doi, const std::string &publication_id) {
    db_writer->queryOrDie("UPDATE tuefind_publications SET doi = " + db_writer->escapeAndQuoteString(doi)
                          + ", doi_notification_datetime = NOW() WHERE id=" + db_writer->escapeAndQuoteString(publication_id));
}


void DownloadAndUpdate(DbConnection * const &db_writer, const std::string &dspace_server_url,
                       const std::vector<std::string> &notification_mail_addresses, const std::string &external_document_guid,
                       const std::string &publication_id) {
    LOG_INFO("Processing ID: : " + publication_id);
    const std::string DOWNLOAD_URL(dspace_server_url + "rest/items/" + external_document_guid + "/metadata");

    Downloader::Params params;
    params.additional_headers_.emplace_back("Accept: application/json"); // Default in DSpace 6 would be XML, but we want JSON

    Downloader downloader(DOWNLOAD_URL, params);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Error while downloading data for id " + publication_id + ": " + downloader.getLastErrorMessage());
        // Since this is a technical problem, the message will always be sent to ixtheo-team only
        // to avoid meistertask items being created in this case.
        EmailSender::SimplerSendEmail(EMAIL_SENDER, "ixtheo-team@ub.uni-tuebingen.de", "Error while downloading data from DSpace API",
                                      "Error while downloading data for id " + publication_id + ": " + downloader.getLastErrorMessage(),
                                      EmailSender::VERY_HIGH);
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

    LOG_INFO("Updating DOI for ID: " + publication_id + " => " + dspace_item.doi_);
    SendNotificationsForItem(dspace_item, notification_mail_addresses);
    UpdateItem(db_writer, dspace_item.doi_, publication_id);
}


} // unnamed namespace


int Main(int argc, char **argv) {
    // Note: It only makes sense to run this program on the live server
    //       because the DSpace test server doesn't register DOIs.
    if (argc != 3)
        Usage();

    const std::string dspace_server_url(argv[1]);
    if (not StringUtil::EndsWith(dspace_server_url, "/"))
        LOG_ERROR("dspace_server_url MUST end with a slash!");

    std::vector<std::string> notification_email_addresses;
    StringUtil::Split(std::string(argv[2]), ';', &notification_email_addresses);

    // We need 2 connections so we can update while iterating a ResultSet.
    auto db_reader((DbConnection::VuFindMySQLFactory()));
    auto db_writer(DbConnection::VuFindMySQLFactory());

    db_reader.queryOrDie("SELECT * FROM tuefind_publications WHERE doi_notification_datetime IS NULL");
    auto result_set(db_reader.getLastResultSet());
    while (const auto row = result_set.getNextRow()) {
        DownloadAndUpdate(&db_writer, dspace_server_url, notification_email_addresses, row["external_document_guid"], row["id"]);
    }

    return EXIT_SUCCESS;
}
