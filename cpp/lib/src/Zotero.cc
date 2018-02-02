/** \brief Interaction with Zotero Translation Server
 *         public functions are named like endpoints
 *         see https://github.com/zotero/translation-server
 *  \author Mario Trojan
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Zotero.h"
#include <uuid/uuid.h>
#include "JSON.h"
#include "StringUtil.h"
#include "util.h"


const std::vector<std::string> Zotero::ExportFormats(
    { "bibtex", "biblatex", "bookmarks", "coins", "csljson", "mods", "refer",
    "rdf_bibliontology", "rdf_dc", "rdf_zotero", "ris", "wikipedia", "tei" }
);


// We try to be unique for the machine we're on.  Beyond that we may have a problem.
std::string Zotero::GetNextSessionId() {
    static unsigned counter;
    static uint32_t uuid[4];
    if (unlikely(counter == 0))
        ::uuid_generate(reinterpret_cast<unsigned char *>(uuid));
    ++counter;
    return "ub_tools_zts_client_" + StringUtil::ToString(uuid[0]) + StringUtil::ToString(uuid[1])
           + StringUtil::ToString(uuid[2]) + StringUtil::ToString(uuid[3]) + "_" + StringUtil::ToString(counter);
}


bool Zotero::Export(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params * const downloader_params,
                    const std::string &format, const std::string &json,
                    std::string * const response_body, std::string * const error_message)
{
    const std::string endpoint_url(Url(zts_server_url.toString() + "/export?format=" + format));
    downloader_params->additional_headers_ = { "Content-Type: application/json" };
    downloader_params->post_data_ = json;

    Downloader downloader(endpoint_url, *downloader_params, time_limit);
    if (downloader.anErrorOccurred()) {
        *error_message = downloader.getLastErrorMessage();
        return false;
    } else {
        *response_body = downloader.getMessageBody();
        return true;
    }
}


bool Zotero::Web(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params * const downloader_params,
                 const Url &harvest_url, const std::string &harvested_html,
                 std::string * const response_body, unsigned * response_code, std::string * const error_message)
{
    const std::string endpoint_url(Url(zts_server_url.toString() + "/web"));
    downloader_params->additional_headers_ = { "Accept: application/json", "Content-Type: application/json" };
    downloader_params->post_data_ = "{\"url\":\"" + JSON::EscapeString(harvest_url) + "\","
                                   + "\"sessionid\":\"" + JSON::EscapeString(GetNextSessionId()) + "\"";
    if (not harvested_html.empty())
        downloader_params->post_data_ += ",\"cachedHTML\":\"" + JSON::EscapeString(harvested_html) + "\"";
    downloader_params->post_data_ += "}";

    Downloader downloader(endpoint_url, *downloader_params, time_limit);
    if (downloader.anErrorOccurred()) {
        *error_message = downloader.getLastErrorMessage();
        return false;
    } else {
        *response_code = downloader.getResponseCode();
        *response_body = downloader.getMessageBody();
        return true;
    }
}
