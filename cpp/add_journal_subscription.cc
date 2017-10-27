/** \file    add_journal_subscription.cc
 *  \brief   Adds one or more subscriptions for a user.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016,2017, Library of the University of Tübingen

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

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "DbConnection.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"
#include "VuFind.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] user_id journal_ppn1 [journal_ppn2 .. journal_ppnN]\n";
    std::exit(EXIT_FAILURE);
}


/** \return The current date and time in the ISO 8601 Zulu format. */
inline std::string ZuluNow() {
    return TimeUtil::GetCurrentDateAndTime(TimeUtil::ZULU_FORMAT);
}


bool SubscriptionExists(DbConnection * const db_connection, const std::string &user_id, const std::string &parent_ppn)
{
    db_connection->queryOrDie("SELECT last_issue_date FROM ixtheo_journal_subscriptions WHERE id=" + user_id
                              + " AND journal_control_number='" + parent_ppn + "'");
    DbResultSet result_set(db_connection->getLastResultSet());
    return not result_set.empty();
}


void AddSubscription(const bool verbose, DbConnection * const db_connection, const std::string &user_id,
                     const std::string &parent_ppn)
{
    if (SubscriptionExists(db_connection, user_id, parent_ppn)) {
        if (verbose)
            std::cout << "Subscription for PPN " << parent_ppn << ", and user ID " << user_id << " already exists!\n";
        return;
    }

    const std::string INSERT_STMT("INSERT INTO ixtheo_journal_subscriptions SET id=" + user_id
                                  + ",last_issue_date='" + ZuluNow() + "',journal_control_number='" + parent_ppn
                                  + "'");
    if (unlikely(not db_connection->query(INSERT_STMT)))
        logger->error("Replace failed: " + INSERT_STMT + " (" + db_connection->getLastErrorMessage() + ")");
}


void AddSubscriptions(const bool verbose, DbConnection * const db_connection, const std::string &user_id,
                      const std::vector<std::string> &parent_ppns)
{
    db_connection->queryOrDie("SELECT id FROM user WHERE id=" + user_id);
    DbResultSet id_result_set(db_connection->getLastResultSet());
    if (id_result_set.empty())
        logger->error(user_id + " is an unknown user ID!");

    for (const auto &parent_ppn : parent_ppns)
        AddSubscription(verbose, db_connection, user_id, parent_ppn);
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 3)
        Usage();

    bool verbose(false);
    if (std::strcmp("--verbose", argv[1]) == 0) {
        verbose = true;
        --argc, ++argv;
    }

    if (argc < 3)
        Usage();

    const std::string user_id(argv[1]);
    if (not StringUtil::IsUnsignedNumber(user_id))
        logger->error("\"" + user_id + "\" is not a valid numeric user ID!");

    std::vector<std::string> parent_ppns;
    for (int arg_no(2); arg_no < argc; ++arg_no) {
        const std::string ppn_candidate(argv[arg_no]);
        if (not MiscUtil::IsValidPPN(ppn_candidate))
            logger->error("\"" + ppn_candidate + "\" is not a valid PPN!");
        parent_ppns.emplace_back(ppn_candidate);
    }

    try {
        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);
        DbConnection db_connection(mysql_url);

        AddSubscriptions(verbose, &db_connection, user_id, parent_ppns);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
