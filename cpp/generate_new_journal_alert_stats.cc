/** \file    generate_new_journal_alert_stats.cc
 *  \brief   Generates a statics report for our journal alerts.
 *  \note    Additional documentation can be found at
 *           https://github.com/ubtue/ub_tools/wiki/Abonnementservice-f%C3%BCr-Zeitschriftenartikel-in-IxTheo-und-RelBib
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2021 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <vector>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "DbConnection.h"
#include "EmailSender.h"
#include "DnsUtil.h"
#include "IniFile.h"
#include "Solr.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"
#include "VuFind.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[solr_host_and_port] user_type hostname email_recipient\n"
            "  Sends out notification emails for journal subscribers.\n"
            "  Should \"solr_host_and_port\" be missing \"" + Solr::DEFAULT_HOST + ":" + std::to_string(Solr::DEFAULT_PORT) + "\" will be used.\n"
            "  \"user_type\" must be \"ixtheo\", \"relbib\" or some other realm."
            "  \"hostname\" should be the symbolic hostname which will be used in constructing\n"
            "  URL's that a user might see.\n"
            "  If \"--debug\" is given, emails will not be sent and database will not be updated.\n");
}


struct Stats {
    unsigned no_of_users_with_subscriptions_;
    double average_number_of_bundle_subscriptions_;
    double average_subscriptions_per_user_;
    unsigned no_of_subscribed_journals_;
    unsigned no_of_subscribed_journals_with_notifications_;
    double average_number_of_notified_articles_per_subscribed_journal_;
    unsigned report_interval_in_days_;
};


inline bool IsBundle(const std::string &serial_control_number) {
    if (serial_control_number.empty())
        return false;
    return not std::isdigit(serial_control_number[0]);
}


void CollectStats(DbConnection * const db_connection, const std::string &user_type, Stats * const stats) {
    db_connection->queryOrDie("SELECT DISTINCT user_id FROM ixtheo_journal_subscriptions WHERE user_id IN (SELECT id FROM "
                              "ixtheo_user WHERE ixtheo_user.user_type = '" + user_type  + "')");
    auto user_ids_result_set(db_connection->getLastResultSet());
    stats->no_of_users_with_subscriptions_ = user_ids_result_set.size();

    const IniFile bundles_config(UBTools::GetTuelibPath() + "journal_alert_bundles.conf");
    unsigned no_of_individual_subscriptions(0), no_of_bundle_subscriptions(0);
    while (const auto user_id_row = user_ids_result_set.getNextRow()) {
        const auto user_id(user_id_row["user_id"]);
        db_connection->queryOrDie("SELECT journal_control_number_or_bundle_name FROM "
                                  "ixtheo_journal_subscriptions WHERE user_id=" + user_id);
        auto journal_control_number_or_bundle_name_result_set(db_connection->getLastResultSet());
        while (const auto journal_control_number_or_bundle_name_row
               = journal_control_number_or_bundle_name_result_set.getNextRow())
        {
            const auto journal_control_number_or_bundle_name(
                journal_control_number_or_bundle_name_row["journal_control_number_or_bundle_name"]);
            if (IsBundle(journal_control_number_or_bundle_name)) {
                ++no_of_bundle_subscriptions;
                const auto bundle_section(bundles_config.getSection(journal_control_number_or_bundle_name));
                if (unlikely(bundle_section == bundles_config.end()))
                    LOG_ERROR("bundle \"" + journal_control_number_or_bundle_name + "\" not found in \""
                              + bundles_config.getFilename() + "\"!");
                no_of_individual_subscriptions += bundle_section->size();
            }
        }
    }

    stats->average_number_of_bundle_subscriptions_ = double(no_of_bundle_subscriptions) / stats->no_of_users_with_subscriptions_;
    stats->average_subscriptions_per_user_ = double(no_of_individual_subscriptions) / stats->no_of_users_with_subscriptions_;
}


void GenerateAndMailReport(const std::string &email_address, const Stats &stats) {
    if (EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", email_address, "Journal Alert Stats",
                                      "Host: " + DnsUtil::GetHostname() + "\n"
                                      + "Report interval in days: " + std::to_string(stats.report_interval_in_days_) + "\n"
                                      + "Number of users w/ subscriptions: "
                                      + std::to_string(stats.no_of_users_with_subscriptions_) + "\n"
                                      + "Average number of subscriptions per user: "
                                      + StringUtil::ToString(stats.average_subscriptions_per_user_, 3) + "\n"
                                      + "Average number of bundle subscriptions per user: "
                                      + StringUtil::ToString(stats.average_number_of_bundle_subscriptions_, 3) + "\n"
                                      + "Total number of currently subscribed journals: "
                                      + std::to_string(stats.no_of_subscribed_journals_) + "\n"
                                      + "Number of subscribed journals w/ no notifications: "
                                      + std::to_string(stats.no_of_subscribed_journals_
                                                       - stats.no_of_subscribed_journals_with_notifications_) + "\n"
                                      + "Average number of notified articles per subscribed journal: "
                                      + StringUtil::ToString(stats.average_number_of_notified_articles_per_subscribed_journal_))
        > 299)
        LOG_ERROR("failed to send an email report to \"" + email_address + "\"!");
}


} // unnamed namespace


// gets user subscriptions for superior works from MySQL
// uses a KeyValueDB instance to prevent entries from being sent multiple times to same user
int Main(int argc, char **argv) {
    if (argc != 4 and argc != 5)
        Usage();

    std::string solr_host_and_port;
    if (argc == 3)
        solr_host_and_port = Solr::DEFAULT_HOST + ":" + std::to_string(Solr::DEFAULT_PORT);
    else {
        solr_host_and_port = argv[1];
        --argc, ++argv;
    }

    const std::string user_type(argv[1]);
    if (user_type != "ixtheo" and user_type != "relbib")
        LOG_ERROR("user_type parameter must be either \"ixtheo\" or \"relbib\"!");

    const std::string hostname(argv[2]);
    const std::string email_recipient(argv[3]);

    std::shared_ptr<DbConnection> db_connection(VuFind::GetDbConnection());

    Stats stats;
    CollectStats(db_connection.get(), user_type, &stats);
    GenerateAndMailReport(email_recipient, stats);

    return EXIT_SUCCESS;
}
