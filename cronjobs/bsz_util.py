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


# Check whether all the instances are needed
def NeedsBothInstances(filename_regex):
     without_localdata_pattern = "_o[)]?-[(]?.*[)]?"
     without_localdata_regex = re.compile(without_localdata_pattern)
     return without_localdata_regex.search(filename_regex.pattern) is not None



# For IxTheo our setup requires that we obtain both the files with and without local data because otherwise
# we get data inconsistencies
def AreBothInstancesPresent(filename_regex, remote_files):
     if not remote_files:
        return True
     matching_remote_files = list(filter(filename_regex.match, remote_files))
     return True if len(matching_remote_files) % 2 == 0 else False



# Downloads matching files found in "remote_directory" on the FTP server that have a datestamp
# more recent than "download_cutoff_date" if some consistency check succeeds
def DownloadRemoteFiles(config, ftp, filename_regex, remote_directory, download_cutoff_date):
    filenames = GetListOfRemoteFiles(ftp, filename_regex, remote_directory, download_cutoff_date)
    if NeedsBothInstances(filename_regex):
        if not AreBothInstancesPresent(filename_regex, filenames):
            util.Error("Skip downloading since apparently generation of the files on the FTP server is not complete!")
    for filename in filenames:
        DownLoadFile(ftp, filename)
    return filenames


# Extracts the first 6 digit sequence in "filename" hoping it corresponds to a YYMMDD pattern.
def ExtractDateFromFilename(filename):
    date_extraction_regex = re.compile(".+(\\d{6}).+")
    match = date_extraction_regex.match(filename)
    if not match:
        util.Error("\"" + filename + "\" does not contain a date!")
    return match.group(1)


def GetCutoffDateForDownloads(config):
    backup_directory = GetBackupDirectoryPath(config)
    most_recent_backup_file = GetMostRecentLocalFile(re.compile(".+(\\d{6}).+"), backup_directory)
    if most_recent_backup_file is None:
        return "000000"
    else:
        return ExtractDateFromFilename(most_recent_backup_file)


def GetFilenameRegexForSection(config, section):
    try:
        filename_pattern = config.get(section, "filename_pattern")
    except Exception as e:
        util.Error("Invalid section " + section + "in config file! (" + str(e) + ")")
    try:
        filename_regex = re.compile(filename_pattern)
    except Exception as e:
        util.Error("filename pattern \"" + filename_pattern + "\" failed to compile! ("
                   + str(e) + ")")
    return filename_regex
