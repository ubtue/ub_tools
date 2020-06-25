/** \brief Checks the BSZ delivery database to find journals for which we have no reasonably new articles delivered.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <ctime>
#include <cstdlib>
#include "DbConnection.h"
#include "EmailSender.h"
#include "IniFile.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"
#include "ZoteroHarvesterConfig.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--min-log-level=log_level] [--default-update-window=no_of_days] config_file_path sender_email_address "
            "notification_email_address");
}


void ProcessJournal(DbConnection * const db_connection, const std::string &journal_name, const std::string &zeder_id,
                    const std::string &zeder_instance, const  unsigned update_window, std::string * tardy_list)
{
    db_connection->queryOrDie("SELECT MAX(delivered_at) AS max_delivered_at FROM delivered_marc_records WHERE zeder_id="
                              + db_connection->escapeAndQuoteString(zeder_id) + " and zeder_instance="
                              + db_connection->escapeAndQuoteString(zeder_instance));

    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        return;

    const std::string max_delivered_at_string(result_set.getNextRow()["max_delivered_at"]);
    const time_t max_delivered_at(SqlUtil::DatetimeToTimeT(max_delivered_at_string));

    if (max_delivered_at < ::time(nullptr) - update_window * 86400)
        *tardy_list += journal_name + ": " + max_delivered_at_string + "\n";
}


const unsigned DEFAULT_UPDATE_WINDOW(60); // days


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        Usage();

    unsigned default_update_window(DEFAULT_UPDATE_WINDOW);
    if (StringUtil::StartsWith(argv[1], "--default-update-window=")) {
        if (not StringUtil::ToUnsigned(argv[1] + __builtin_strlen("--default-update-window=")))
            LOG_ERROR("invalid default update window: \"" + std::string(argv[1] + __builtin_strlen("--default-update-window=")) + "\"!");
        --argc, ++argv;
    }

    if (argc != 4)
        Usage();

    const std::string sender_email_address(argv[2]), notification_email_address(argv[3]);
    DbConnection db_connection;

    IniFile ini_file(UBTools::GetTuelibPath() + "zotero-enhancement-maps/zotero_harvester.conf");
    std::string tardy_list;
    for (const auto &section : ini_file) {
        if (section.find("user_agent") != section.end())
            continue; // Not a journal section.

        const auto delivery_mode(section.getEnum("zotero_delivery_mode", ZoteroHarvester::Config::STRING_TO_UPLOAD_OPERATION_MAP,
                                                 ZoteroHarvester::Config::UploadOperation::NONE));
        if (delivery_mode != ZoteroHarvester::Config::UploadOperation::LIVE or section.getBool("zeder_newly_synced_entry", false))
            continue;

        const std::string journal_name(section.getSectionName());

        const std::string zeder_id(section.getString("zeder_id"));
        const std::string zeder_instance(TextUtil::UTF8ToLower(section.getString("zotero_group")));

        unsigned update_window;
        if (section.find("zeder_update_window") == section.end()) {
            LOG_WARNING("no update window found for \"" + journal_name + "\", using " + std::to_string(default_update_window) + "!");
            update_window = default_update_window;
        } else
            update_window = section.getUnsigned("update_window");

        ProcessJournal(&db_connection, journal_name, zeder_id, zeder_instance, update_window, &tardy_list);
    }

    if (not tardy_list.empty()) {
        if (EmailSender::SendEmail(sender_email_address, notification_email_address, "Überfällige Zeitschiften",
                                   "Letzte Lieferung ans BSZ\n" + tardy_list, EmailSender::HIGH) > 299)
            LOG_ERROR("failed to send email notofication!");
    }

    return EXIT_SUCCESS;
}
