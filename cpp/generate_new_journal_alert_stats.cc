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
#include "BinaryIO.h"
#include "Compiler.h"
#include "DbConnection.h"
#include "DnsUtil.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "Solr.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "util.h"
#include "VuFind.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[solr_host_and_port] user_type report_interval_in_days email\n"
            "  Generates a CSV report about journal subscription statistics.\n"
            "  Should \"solr_host_and_port\" be missing \"" + Solr::DEFAULT_HOST + ":"
            + std::to_string(Solr::DEFAULT_PORT) + "\" will be used.\n"
            "  \"user_type\" must be \"ixtheo\", \"relbib\" or some other realm.\n"
            "  \"report_interval_in_days\" can be a number or the text \"days_in_last_month\n"
            "  \"email\" recipient email address.\n");
}


struct Stats {
    unsigned no_of_users_with_subscriptions_;
    double average_number_of_bundle_subscriptions_;
    double average_subscriptions_per_user_;
    unsigned no_of_subscribed_journals_;
    unsigned no_of_journals_for_which_notifications_were_sent_;
    unsigned no_of_subscribed_journals_with_notifications_;
    double average_number_of_notified_articles_per_notified_journal_;
    unsigned report_interval_in_days_;
};


inline bool IsBundle(const std::string &serial_control_number) {
    if (serial_control_number.empty())
        return false;
    return not std::isdigit(serial_control_number[0]);
}


size_t GetBundleSize(const IniFile &bundles_config, const std::string &bundle_name) {
    static std::map<std::string, size_t> bundle_names_to_sizes_map;
    const auto bundle_name_and_size(bundle_names_to_sizes_map.find(bundle_name));
    if (bundle_name_and_size != bundle_names_to_sizes_map.cend())
        return bundle_name_and_size->second;

    const std::string bundle_ppns_string(bundles_config.getString(bundle_name, "ppns", ""));
    if (unlikely(bundle_ppns_string.empty()))
        LOG_ERROR("bundle \"" + bundle_name + "\" not found in \"" + bundles_config.getFilename() + "\"!");

    std::vector<std::string> bundle_ppns;
    StringUtil::SplitThenTrim(bundle_ppns_string, "," , " \t", &bundle_ppns);
    bundle_names_to_sizes_map[bundle_name] = bundle_ppns.size();

    return bundle_ppns.size();
}


void CollectConfigStats(DbConnection * const db_connection, const std::string &user_type, Stats * const stats) {
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
                no_of_individual_subscriptions += GetBundleSize(bundles_config, journal_control_number_or_bundle_name);
            }
        }
    }

    stats->average_number_of_bundle_subscriptions_ = double(no_of_bundle_subscriptions) / stats->no_of_users_with_subscriptions_;
    stats->average_subscriptions_per_user_ = double(no_of_individual_subscriptions) / stats->no_of_users_with_subscriptions_;
}


void CollectUsageStats(const std::string &user_type, Stats * const stats) {
    const auto USAGE_STATS_PATH(UBTools::GetTuelibPath() + "new_journal_alert.stats");
    const auto usage_stats_file(FileUtil::OpenInputFileOrDie(USAGE_STATS_PATH));

    const double NOW(TimeUtil::GetJulianDayNumber());
    const double TIME_WINDOW(NOW - stats->report_interval_in_days_);

    stats->no_of_subscribed_journals_with_notifications_ = 0;
    stats->average_number_of_notified_articles_per_notified_journal_ = 0.0;
    std::unordered_set<std::string> seen_superior_ppns;
    const auto USAGE_STATS_FILE_SIZE(usage_stats_file->size());
    while (usage_stats_file->tell() < USAGE_STATS_FILE_SIZE) {
        double julian_day_number;
        BinaryIO::ReadOrDie(*usage_stats_file, &julian_day_number);
        std::string logged_user_type;
        BinaryIO::ReadOrDie(*usage_stats_file, &logged_user_type);
        std::string journal_ppn;
        BinaryIO::ReadOrDie(*usage_stats_file, &journal_ppn);
        unsigned notified_count;
        BinaryIO::ReadOrDie(*usage_stats_file, &notified_count);

        if (julian_day_number > TIME_WINDOW and logged_user_type == user_type) {
            if (seen_superior_ppns.find(journal_ppn) == seen_superior_ppns.end()) {
                seen_superior_ppns.emplace(journal_ppn);
                ++(stats->no_of_subscribed_journals_with_notifications_);
            }
            stats->average_number_of_notified_articles_per_notified_journal_ += notified_count;
        }
    }

    stats->average_number_of_notified_articles_per_notified_journal_ /=
        stats->no_of_subscribed_journals_with_notifications_;
}


void GenerateReport(File * const report_file, const Stats &stats) {
    (*report_file) << "\"Report interval in days\"," << stats.report_interval_in_days_ << '\n'
                   << "\"Number of users w/ subscriptions\"," << stats.no_of_users_with_subscriptions_ << '\n'
                   << "\"Average number of subscriptions per user\"," << stats.average_subscriptions_per_user_ << '\n'
                   << "\"Average number of bundle subscriptions per user\","
                   << stats.average_number_of_bundle_subscriptions_ << '\n'
                   << "\"Total number of currently subscribed journals\"," << stats.no_of_subscribed_journals_ << '\n'
                   << "\"Number of subscribed journals w/ notifications\","
                   << stats.no_of_subscribed_journals_with_notifications_ << '\n'
                   << "\"Average number of notified articles per notified journal\","
                   << stats.average_number_of_notified_articles_per_notified_journal_ << '\n';
}


} // unnamed namespace


const std::string REPORT_DIRECTORY("/mnt/ZE020110/FID-Projekte/Statistik/"); // Must end w/ a slash!


// gets user subscriptions for superior works from MySQL
// uses a KeyValueDB instance to prevent entries from being sent multiple times to same user
int Main(int argc, char **argv) {
    if (argc != 4 and argc != 5)
        Usage();

    std::string solr_host_and_port;
    if (argc == 4)
        solr_host_and_port = Solr::DEFAULT_HOST + ":" + std::to_string(Solr::DEFAULT_PORT);
    else {
        solr_host_and_port = argv[1];
        --argc, ++argv;
    }

    const std::string user_type(argv[1]);
    if (user_type != "ixtheo" and user_type != "relbib")
        LOG_ERROR("user_type parameter must be either \"ixtheo\" or \"relbib\"!");

    Stats stats;
    if (std::strcmp(argv[2], "days_in_last_month") != 0)
        stats.report_interval_in_days_ = StringUtil::ToUnsigned(argv[2]);
    else {
        unsigned year, month, day;
        TimeUtil::GetCurrentDate(&year, &month, &day);
        if (month != 1)
            --month;
        else {
            month = 12;
            --year;
        }
        stats.report_interval_in_days_ = TimeUtil::GetDaysInMonth(year, month);
    }

    const std::string email_recipient(argv[3]);
    std::shared_ptr<DbConnection> db_connection(VuFind::GetDbConnection());
    CollectConfigStats(db_connection.get(), user_type, &stats);
    CollectUsageStats(user_type, &stats);

    const auto report_file(FileUtil::OpenOutputFileOrDie(REPORT_DIRECTORY + "new_journal_alert_stats."
                                                         + DnsUtil::GetHostname() + "." + user_type
                                                         + "." + TimeUtil::GetCurrentDateAndTime("%Y-%m-%d") + ".csv"));
    GenerateReport(report_file.get(), stats);

    return EXIT_SUCCESS;
}
