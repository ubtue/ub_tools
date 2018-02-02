/** \brief Interaction with Zotero Translation Server
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
    static std::string GetNextSessionId();
public:
    static bool Web(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params * const downloader_params,
                    const Url &harvest_url, const std::string &harvested_html,
                    std::string * const response_body, unsigned * response_code, std::string * const error_message);


}; // class Zotero


#endif // ifndef ZOTERO_H
