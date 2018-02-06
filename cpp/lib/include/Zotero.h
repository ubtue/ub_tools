/** \brief Interaction with Zotero Translation Server
 *         - public functions are named like endpoints, see
 *           https://github.com/zotero/translation-server
 *         - For a list of Zotero field types ("itemFields") in JSON, see
 *           https://github.com/zotero/zotero/blob/master/chrome/locale/de/zotero/zotero.properties#L409
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
#ifndef ZOTERO_H
#define ZOTERO_H


#include "Downloader.h"
#include "TimeLimit.h"
#include "Url.h"


class Zotero {
public:
    // native supported formats, see https://github.com/zotero/translation-server/blob/master/src/server_translation.js#L31-43
    static const std::vector<std::string> ExportFormats;

    /** \brief Use builtin translator to convert JSON to output format. */
    static bool Export(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
                       const std::string &format, const std::string &json,
                       std::string * const response_body, std::string * const error_message);

    /** \brief Download URL and return as JSON. (If harvested_html is given, URL is not downloaded again. ) */
    static bool Web(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
                    const Url &harvest_url, const std::string &harvested_html,
                    std::string * const response_body, unsigned * response_code, std::string * const error_message);
private:
    static std::string GetNextSessionId();
}; // class Zotero


#endif // ifndef ZOTERO_H
