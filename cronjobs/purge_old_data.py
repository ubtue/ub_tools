#!/bin/python3
# -*- coding: utf-8 -*-
#
# A tool that removes old data files that contain a YYMMDD timestamp in the filename.
# Config files for this tool look like this:
"""
[SMTPServer]
server_address  = smtpserv.uni-tuebingen.de
server_user     = XXXXXX
server_password = XXXXXX

[PurgeFiles]
generations_to_keep = 2
"""

import glob
import os
import re
import sys
import traceback
import util


def PurgeFiles(generations_to_keep, all_files):
    regex = re.compile("\\d{6}")
    map_to_lists = {}
    for file_name in all_files:
        normalised_name = regex.sub("YYMMDD", file_name)
        if normalised_name in map_to_lists:
            file_name_list = map_to_lists[normalised_name]
            file_name_list.append(file_name)
            map_to_lists[normalised_name] = file_name_list
        else:
            map_to_lists[normalised_name] = [file_name]

    deleted_names = ""
    for _, file_name_list in list(map_to_lists.items()):
        if len(file_name_list) <= generations_to_keep:
            continue
        file_name_list = sorted(file_name_list, reverse=True)
        while len(file_name_list) > generations_to_keep:
            name_to_delete = file_name_list.pop()
            os.unlink(name_to_delete)
            deleted_names += name_to_delete + "\n"

    if deleted_names:
        util.SendEmail("File Purge", "Deleted:\n" + deleted_names, priority=5)
    else:
        util.SendEmail("File Purge", "Found no files to purge.\n", priority=5)


def Main():
    if len(sys.argv) != 2:
        util.Error("This script expects one argument: default_email_recipient")
    util.default_email_recipient = sys.argv[1]
    config = util.LoadConfigFile()

    try:
        generations_to_keep = config.getint("PurgeFiles", "generations_to_keep")
    except Exception as e:
        util.Error("failed to read config file! ("+ str(e) + ")")
    if generations_to_keep < 1:
        util.Error("generations_to_keep must be at least 1!")

    all_timestamped_files = glob.glob("*[0-9][0-9][0-9][0-9][0-9][0-9]*")
    if not all_timestamped_files:
        util.SendEmail("File Purge Failed", "No timestamped files found!", priority=1)
    PurgeFiles(generations_to_keep, all_timestamped_files)


try:
    Main()
except Exception as e:
    util.SendEmail("File Purge Failed", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
