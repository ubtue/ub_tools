# Python 3 module
# -*- coding: utf-8 -*-

from ftplib import FTP
import datetime
import os
import re
import sys
import time
import traceback
import util
import shutil
import string
import tempfile


def FoundNewBSZDataFile(link_filename):
    try:
        statinfo = os.stat(link_filename)
        file_creation_time = statinfo.st_ctime
    except OSError as e:
        util.Error("in FoundNewBSZDataFile: Symlink \"" + link_filename + "\" is missing or dangling!")
    old_timestamp = util.ReadTimestamp()
    return old_timestamp < file_creation_time


# Returns a list of files found in the "directory" directory on an FTP server that match "filename_regex"
# and have a datestamp (YYMMDD) more recent than "download_cutoff_date".
def GetListOfRemoteFiles(ftp, filename_regex, directory, download_cutoff_date):
    try:
        ftp.cwd(directory)
    except Exception as e:
        util.Error("can't change directory to \"" + directory + "\"! (" + str(e) + ")")

    # Retry calling GetMostRecentFile() up to 3 times:
    exception = None
    for attempt_number in range(3):
        try:
            filename_list = []
            for filename in ftp.nlst():
                 match = filename_regex.match(filename)
                 if match and match.group(1) >= download_cutoff_date:
                     filename_list.append(filename)
            return filename_list
        except Exception as e:
            exception = e
            time.sleep(10 * (attempt_number + 1))
    raise exception


def GetMostRecentFile(filename_regex, filename_generator):
    most_recent_date = "000000"
    most_recent_file = None
    for filename in filename_generator:
        match = filename_regex.match(filename)
        if match and match.group(1) > most_recent_date:
            most_recent_date = match.group(1)
            most_recent_file = filename
    return most_recent_file


def GetMostRecentLocalFile(filename_regex, local_directory=None):
    def LocalFilenameGenerator():
        if local_directory is None:
            return os.listdir(".")
        else:
            return os.listdir(local_directory)

    return GetMostRecentFile(filename_regex, LocalFilenameGenerator())


def GetBackupDirectoryPath(config):
    try:
        backup_directory = config.get("Kumulierte Abzuege", "output_directory")
    except Exception as e:
        util.Error("could not determine output directory (" + str(e) + ")")

    if not os.path.exists(backup_directory):
        util.Error("backup directory is missing: \"" + backup_directory + "\"!")

    return backup_directory
