/** \file    bulk_mailer.cc
 *  \brief   Sends emails to a list of users.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2020-2021 Tübinge University Libarry

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
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("mail_contents sql_statement auxillary_email_address_list sender_and_reply_to_email_address\n"
            "\tmail_contents: the file containing the email message\n"
            "\t\t(the first line will be used as the mail's subject)\n"
            "\tsql_statement: a query to select the relevant email addresses from the vufind user table\n"
            "\t\t(the email addresses will be selected as the first column of the query result set)\n"
            "\tauxillary_email_address_list: a path to a plain-text file containing one email address per line\n"
            "\tsender_and_reply_to_email_address: the email address that will be set as the sender and the reply_to field\n");
}


void CollectRecipientsFromSqlTable(const std::string &select_statment, std::vector<std::string> * const recipients) {
    const auto old_size(recipients->size());

    auto db_connection(DbConnection::VuFindMySQLFactory());
    db_connection.queryOrDie(select_statment);
    DbResultSet result_set(db_connection.getLastResultSet());
    DbRow row;
    while (row = result_set.getNextRow())
        recipients->emplace_back(row[0]);

    LOG_INFO("Collected " + std::to_string(recipients->size() - old_size)
             + " recipients from the output of the database query.");
}


void CollectRecipientsFromFile(const std::string &filename, std::vector<std::string> * const recipients) {
    const auto old_size(recipients->size());

    for (auto line : FileUtil::ReadLines(filename))
        recipients->emplace_back(line);

    LOG_INFO("Collected " + std::to_string(recipients->size() - old_size) + " from " + filename + ".");
}


void SendAllEmails(const std::string &message_file, const std::string &sender_and_reply_to_address, const std::vector<std::string> &recipients) {
    std::string message(EmailSender::NormaliseLineEnds(FileUtil::ReadStringOrDie(message_file)));
    const auto first_line_end_pos(message.find("\r\n"));
    if (first_line_end_pos == std::string::npos)
        LOG_ERROR("Missing subject line in \"" + message_file + "\"!");
    const std::string subject(message.substr(0, first_line_end_pos));
    message = message.substr(first_line_end_pos + 2 /* skip over CR and NL */);

    unsigned success_count(0), failure_count(0);
    for (const auto &recipient : recipients) {
        unsigned short response_code;
        if (response_code = EmailSender::SimplerSendEmail(sender_and_reply_to_address, { recipient }, subject, message) <= 299)
            ++success_count;
        else {
            LOG_WARNING("Failed to send to \"" + recipient + "\"! (" + EmailSender::SMTPResponseCodeToString(response_code) + ")");
            ++failure_count;
        }
    }

    LOG_INFO("Successfully sent " + std::to_string(success_count) + " email(s).");
    LOG_INFO(std::to_string(failure_count) + " failure(s) occurred!");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 5)
        Usage();

    std::vector<std::string> recipients;
    CollectRecipientsFromSqlTable(argv[2], &recipients);
    CollectRecipientsFromFile(argv[3], &recipients);
    SendAllEmails(argv[1], argv[4], recipients);

    return EXIT_SUCCESS;
}
