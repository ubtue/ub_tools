#!/bin/python3
# -*- coding: utf-8 -*-
import datetime
import os
import re
import sys
import traceback
import util
import socket
import shutil
import subprocess


"""
[Database]
sql_database = "ixtheo"
sql_username = "ixtheo"
sql_password = "XXXXX"
"""


util.default_email_recipient = "ixtheo-team@ub.uni-tuebingen.de"
period_of_days = 7

def GetNewKeywordNumberFromDB(database, user, password, days_diff):
    end_date = (datetime.datetime.today() + datetime.timedelta(days=1)).strftime("%Y-%m-%d")
    start_date = (datetime.datetime.today() - datetime.timedelta(days=days_diff)).strftime("%Y-%m-%d")
    result_keyword = subprocess.getoutput("/usr/bin/mysql --database " + database + " --user=" + user + " --password=" + password +
            " -sN --execute='SELECT COUNT(*) FROM keyword_translations WHERE create_timestamp>=\"" + start_date + "\" AND create_timestamp<\"" +
            end_date + "\" AND ppn IN (SELECT ppn FROM keyword_translations GROUP BY ppn HAVING COUNT(*)=1);' 2>/dev/null")
    result_vufind = subprocess.getoutput("/usr/bin/mysql --database " + database + " --user=" + user + " --password=" + password +
            " -sN --execute='SELECT COUNT(*) FROM vufind_translations WHERE create_timestamp>=\"" + start_date + "\" AND create_timestamp<\"" +
            end_date + "\" AND token IN (SELECT token FROM vufind_translations GROUP BY token HAVING COUNT(*)=1);' 2>/dev/null")
    return [result_keyword, result_vufind]


def NotifyMailingList(recipient, server, content):
    util.SendEmailBase("New IxTheo keywords ready for translation",
            content + "\nplease have a look at:\nhttps://" + server + "/" +
                    "cgi-bin/translator" + "\n\n" + "Kind regards",
                    None, recipient)


def DetermineServerName():
    return socket.gethostname()


def Main():
    if len(sys.argv) != 2:
        util.SendEmail(os.path.basename(sys.argv[0]),
                      "This script requires an email address for the translators mailing list as the only argument\n", priority=1)
        sys.exit(-1)
    mailing_list_recipient = sys.argv[1]
    try:
        sql_config = util.LoadConfigFile("/usr/local/var/lib/tuelib/translations.conf")
        sql_database = sql_config.get("Database", "sql_database")
        sql_username = sql_config.get("Database", "sql_username")
        sql_password = sql_config.get("Database", "sql_password")
    except Exception as e:
        util.Error("Failed to read sql_config file (" + str(e) + ")")

    [result_keyword, result_vufind] = GetNewKeywordNumberFromDB(sql_database, sql_username, sql_password, period_of_days)
    result = result_keyword + " new keywords and " + result_vufind + " new vufind strings found in the last " + str(period_of_days) + " days"
    servername = DetermineServerName()
    if result_keyword != "0" or result_vufind != "0":
        NotifyMailingList(mailing_list_recipient, servername, result)


try:
    Main()
except Exception as e:
    util.SendEmail("Main", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)

