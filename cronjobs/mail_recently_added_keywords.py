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


util.default_email_recipient = "andreas.nutz@uni-tuebingen.de"
date = datetime.datetime.today().strftime("%y%m%d")

def GetNewKeywordNumberFromDB(database, user, password):
    result_keyword = subprocess.getoutput("/usr/bin/mysql --database " + database + " --user=" + user + " --password=" + password + 
            " -sN --execute='SELECT COUNT(*) FROM keyword_translations;'")
    result_vufind = subprocess.getoutput("/usr/bin/mysql --database " + database + " --user=" + user + " --password=" + password + 
            " -sN --execute='SELECT COUNT(*) FROM vufind_translations;'")
    return result_keyword + " new keywords and " + result_vufind + " new vufind strings found in the last period"


def NotifyMailingList(user, server, content):
    util.SendEmail("New IxTheo keywords ready for translation",
            content + "\nplease have a look at:\nhttps://" + server + "/" +
                    "cgi-bin/translator" + "\n\n" + "Kind regards")


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

    result = GetNewKeywordNumberFromDB(sql_database, sql_username, sql_password)
    servername = DetermineServerName()
    NotifyMailingList(mailing_list_recipient, servername, result)


try:
    Main()
except Exception as e:
    util.SendEmail("Main", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)

