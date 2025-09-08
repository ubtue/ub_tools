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


"""
[Database]
sql_database = "ixtheo"
sql_username = "ixtheo"
sql_password = "XXXXX"
"""


util.default_email_recipient = "johannes.riedl@uni-tuebingen.de"
tmp_file_path = "/tmp"
date = datetime.datetime.today().strftime("%y%m%d")
output_filename = "ixtheo_translations-" + date + ".sql"
web_server_path = "/usr/local/vufind/public"
compressed_extension = ".7z"
encrypted_extension = ".enc"


def ClearOutFile(outfile_name):
    outfile = open(outfile_name, 'w')
    outfile.truncate()
    outfile.close()


def DumpTranslationsDB(database, user, password, outfile_name):
    ClearOutFile(outfile_name)
    util.ExecOrDie("/usr/bin/mysqldump",
                   [ "--single-transaction", "--databases", re.sub('^"|"$', '', database),
                     "--user=" + re.sub('^"|"$', '', user),
                     "--password=" + re.sub('^"|"$', '', password) ],
                     outfile_name)


def CompressAndEncryptFile(infile, outfile, archive_password):
    util.ExecOrDie("/usr/bin/7za", ['a', "-p" + archive_password, outfile, infile])


def MoveToDownloadPosition(file, web_server_path):
    basename = os.path.basename(file)
    shutil.move(file, web_server_path + "/" + basename)


def CleanUp(tmp_file_name):
    os.remove(tmp_file_name)


def NotifyUser(user, server, filename):
    util.SendEmail("New translations database dump provided",
                   "A new translation database dump has been provided for you at https://" + server + "/" +
                    filename + "\n\n" + "Kind regards")


def DetermineServerName():
    return socket.gethostname()


def Main():
    if len(sys.argv) != 2:
        util.SendEmail(os.path.basename(sys.argv[0]),
                      "This script requires an email address as the only argument\n", priority=1,
                      recipient=util.default_email_recipient)
        sys.exit(-1)
    util.default_email_recipient = sys.argv[1]
    user = sys.argv[1]
    try:
        sql_config = util.LoadConfigFile("/usr/local/var/lib/tuelib/translations.conf")
        sql_database = sql_config.get("Database", "sql_database")
        sql_username = sql_config.get("Database", "sql_username")
        sql_password = sql_config.get("Database", "sql_password")
    except Exception as e:
        util.Error("Failed to read sql_config file (" + str(e) + ")")

    try:
        config = util.LoadConfigFile()
        archive_password = config.get("Passwords", "archive_password")
    except Exception as e:
        util.Error("Failed to read config file (" + str(e) + ")")

    raw_dump_file =  tmp_file_path + "/" + output_filename
    DumpTranslationsDB(sql_database, sql_username, sql_password, raw_dump_file)

    compressed_and_encrypted_dump_file = re.sub('\\..*$', '', raw_dump_file) + compressed_extension + encrypted_extension
    CompressAndEncryptFile(raw_dump_file, compressed_and_encrypted_dump_file, re.sub('^"|"$', '', archive_password))

    MoveToDownloadPosition(compressed_and_encrypted_dump_file, web_server_path)
    CleanUp(raw_dump_file)
    servername = DetermineServerName()
    NotifyUser(user, servername, os.path.basename(compressed_and_encrypted_dump_file))


try:
    Main()
except Exception as e:
    util.SendEmail("Main", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)

