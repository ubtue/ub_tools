#!/bin/python3
# -*- coding: utf-8 -*-
#
# A tool for the automation of tarball downloads from the BSZ.
# Config files for this tool look like this:
# (in addition, see BSZ.conf and smtp_server.conf)
"""
[Kompletter Abzug]
filename_pattern = ^SA-MARC-ixtheo-(\d\d\d\d\d\d).tar.gz$
directory_on_ftp_server = /ixtheo

[Differenzabzug]
filename_pattern = ^(?:TA-MARC-ixtheo|SA-MARC-ixtheo_o|TA-MARC-ixtheo_o)-(\d\d\d\d\d\d).tar.gz$
directory_on_ftp_server = /ixtheo

[Hinweisabzug]
filename_pattern = ^SA-MARC-ixtheo_hinweis-(\d\d\d\d\d\d).tar.gz$
directory_on_ftp_server = /ixtheo

[Normdatendifferenzabzug]
filename_pattern = ^(?:WA-MARCcomb)-(\d\d\d\d\d\d).tar.gz$
directory_on_ftp_server = /sekkor

[Loeschlisten]
filename_pattern = ^LOEKXP-(\d\d\d\d\d\d)$
directory_on_ftp_server = /sekkor

[Loeschlisten2]
filename_pattern = ^LOEKXP_m-(\d\d\d\d\d\d)$
directory_on_ftp_server = /ixtheo

[Errors]
filename_pattern = ^Errors_ixtheo_(\d\d\d\d\d\d)$
directory_on_ftp_server = /ixtheo

[Kumulierte Abzuege]
output_directory = /usr/local/ub_tools/bsz_daten_cumulated
"""

import bsz_util
import datetime
import os
import re
import shutil
import sys
import tempfile
import traceback
import util


# Returns "yymmdd_string" incremented by one day unless it equals "000000" (= minus infinity).
def IncrementStringDate(yymmdd_string):
    if yymmdd_string == "000000":
        return yymmdd_string
    date = datetime.datetime.strptime(yymmdd_string, "%y%m%d")
    date = date + datetime.timedelta(days=1)
    return date.strftime("%y%m%d")


# Cumulatively saves downloaded data to an external location to have a complete trace of
# the downloaded data. Thus, the complete data should be reconstructible.
def AddToCumulativeCollection(downloaded_files, config):
    try:
       output_directory = config.get("Kumulierte Abzuege", "output_directory")
    except Exception as e:
        util.Error("Extracting output directory failed! (" + str(e) + ")")

    if not os.path.exists(output_directory):
        try:
            os.makedirs(output_directory)
        except Exception as e:
            util.Error("Unable to create output directory! (" + str(e) + ")")

    try:
        for downloaded_file in downloaded_files:
            if not os.path.exists(output_directory + '/' + os.path.basename(downloaded_file)):
                shutil.move(downloaded_file, output_directory)
    except Exception as e:
        util.Error("Adding file to cumulative collection failed! (" + str(e) + ")")

    return None;


def CumulativeFilenameGenerator(output_directory):
     return os.listdir(output_directory)


# We try to keep all differential updates up to and including the last complete data
def CleanUpCumulativeCollection(config):
    backup_directory = bsz_util.GetBackupDirectoryPath(config)
    filename_complete_data_regex = bsz_util.GetFilenameRegexForSection(config, "Kompletter Abzug")
    incremental_authority_data_regex = bsz_util.GetFilenameRegexForSection(config, "Normdatendifferenzabzug")

    # Find the latest complete data file
    try:
        most_recent_complete_data_filename = bsz_util.GetMostRecentFile(filename_complete_data_regex,
                                                               CumulativeFilenameGenerator(backup_directory))
    except Exception as e:
        util.Error("Unable to to determine the most recent complete data file (" + str(e) + ")")

    if most_recent_complete_data_filename is None:
        return None

    # Extract the date
    match = filename_complete_data_regex.match(most_recent_complete_data_filename)
    if match and match.group(1):
        most_recent_complete_data_date = match.group(1)
        # Delete all older Files but skip incremental authority dumps
        DeleteAllFilesOlderThan(most_recent_complete_data_date, backup_directory, incremental_authority_data_regex)
        # Now explicitly delete incremental authority dumps that are too old
        DeleteAllFilesOlderThan(ShiftDateToTenDaysBefore(most_recent_complete_data_date), backup_directory)
    return None


# Test whether we already have a current "Normdatendifferenzabzug"
def CurrentIncrementalAuthorityDumpPresent(config, cutoff_date):
    filename_regex = bsz_util.GetFilenameRegexForSection(config, "Normdatendifferenzabzug")
    cumulative_directory = bsz_util.GetBackupDirectoryPath(config)
    most_recent_incremental_authority_dump = bsz_util.GetMostRecentLocalFile(filename_regex, cumulative_directory)
    if (most_recent_incremental_authority_dump == None):
        return False
    most_recent_file_incremental_authority_date = bsz_util.ExtractDateFromFilename(most_recent_incremental_authority_dump)
    return most_recent_file_incremental_authority_date > cutoff_date


# Delete all files that are older than a given date
def DeleteAllFilesOlderThan(date, directory, exclude_pattern=""):
    filename_pattern = '\\D*?-(\\d{6}).*'
    try:
        filename_regex = re.compile(filename_pattern)
    except Exception as e:
        util.Error("File name pattern \"" + filename_pattern + "\" failed to compile! (" + str(e) + ")")

    try:
        exclude_regex = re.compile(exclude_pattern)
    except Exception as e:
        util.Error("Exclude pattern \"" + exclude_pattern + "\" failed to compile! (" + str(e) + ")")

    for filename in CumulativeFilenameGenerator(directory):
        match = filename_regex.match(filename)
        if match and match.group(1) < date and not exclude_regex.match(filename):
            os.remove(directory + "/" +  match.group())

    return None


def DownloadData(config, section, ftp, download_cutoff_date, msg):
    filename_regex = bsz_util.GetFilenameRegexForSection(config, section)
    directory_on_ftp_server = bsz_util.GetFTPDirectoryForSection(config, section)
    downloaded_files = bsz_util.DownloadRemoteFiles(config, ftp, filename_regex, directory_on_ftp_server, download_cutoff_date)
    if len(downloaded_files) == 0:
        msg.append("No more recent file for pattern \"" + filename_regex.pattern + "\"!\n")
    else:
        msg.append("Successfully downloaded:\n" + '\n'.join(downloaded_files) + '\n')
    return downloaded_files


def DownloadCompleteData(config, ftp, download_cutoff_date, msg):
    downloaded_files = DownloadData(config, "Kompletter Abzug", ftp, download_cutoff_date, msg)
    complete_filename_pattern = config.get("Kompletter Abzug", "filename_pattern")
    if not bsz_util.NeedsBothInstances(re.compile(complete_filename_pattern)):
        if len(downloaded_files) == 1:
            return downloaded_files
        elif len(downloaded_files) == 0:
            return None
        else:
            util.Error("downloaded multiple complete date tar files!")
    else:
        if not downloaded_files:
            return None
        remote_file_date = bsz_util.ExtractDateFromFilename(downloaded_files[0])
        for filename in downloaded_files:
            if bsz_util.ExtractDateFromFilename(filename) != remote_file_date:
               util.error("We have a complete data dump set with differing dates")
        return downloaded_files


def ShiftDateToTenDaysBefore(date_to_shift):
    date = datetime.datetime.strptime(date_to_shift, "%y%m%d")
    return datetime.datetime.strftime(date - datetime.timedelta(days=10), "%y%m%d")


def Main():
    if len(sys.argv) != 2:
        util.SendEmail(os.path.basename(sys.argv[0]),
                       "This script needs to be called with an email address as the only argument!\n", priority=1)
        sys.exit(-1)
    util.default_email_recipient = sys.argv[1]

    try:
        config = util.LoadConfigFile()
    except Exception as e:
        util.Error("failed to read config file! (" + str(e) + ")")

    ftp = bsz_util.GetFTPConnection()
    msg = []
    tempdir = tempfile.TemporaryDirectory()
    bsz_dir = os.getcwd()
    os.chdir(tempdir.name)
    download_cutoff_date = IncrementStringDate(bsz_util.GetCutoffDateForDownloads(config))
    complete_data_filenames = DownloadCompleteData(config, ftp, download_cutoff_date, msg)
    all_downloaded_files = [] if complete_data_filenames == None else complete_data_filenames
    downloaded_at_least_some_new_titles = False
    if complete_data_filenames is not None:
        download_cutoff_date = bsz_util.ExtractDateFromFilename(complete_data_filenames[0])
        downloaded_at_least_some_new_titles = True
        util.Remove("/usr/local/var/lib/tuelib/local_data.sq3") # Must be the same path as in LocalDataDB.cc
    all_downloaded_files += DownloadData(config, "Differenzabzug", ftp, download_cutoff_date, msg)
    if all_downloaded_files is not []:
        downloaded_at_least_some_new_titles = True
    all_downloaded_files += DownloadData(config, "Loeschlisten", ftp, download_cutoff_date, msg)
    if config.has_section("Loeschlisten2"):
        all_downloaded_files += DownloadData(config, "Loeschlisten2", ftp, download_cutoff_date, msg)
    if config.has_section("Hinweisabzug"):
        all_downloaded_files += DownloadData(config, "Hinweisabzug", ftp, "000000", msg)
    if config.has_section("Errors"):
        all_downloaded_files += DownloadData(config, "Errors", ftp, download_cutoff_date, msg)
    incremental_authority_cutoff_date =  ShiftDateToTenDaysBefore(download_cutoff_date)
    if config.has_section("Normdatendifferenzabzug"):
       if (not CurrentIncrementalAuthorityDumpPresent(config, incremental_authority_cutoff_date)):
           all_downloaded_files += DownloadData(config, "Normdatendifferenzabzug", ftp, incremental_authority_cutoff_date, msg)
       else:
           msg.append("Skipping Download of \"Normdatendifferenzabzug\" since already present\n")
    try:
        for downloaded_file in all_downloaded_files:
            shutil.copy(downloaded_file, bsz_dir)
    except Exception as e:
        util.Error("Moving a downloaded file to the BSZ download directory failed! (" + str(e) + ")")

    AddToCumulativeCollection(all_downloaded_files, config)
    CleanUpCumulativeCollection(config)
    if downloaded_at_least_some_new_titles:
        util.Touch("/usr/local/var/tmp/bsz_download_happened") # Must be the same path as in the merge script and in trigger_pipeline_script.sh
    util.SendEmail("BSZ File Update", ''.join(msg), priority=5)


try:
    Main()
except Exception as e:
    util.SendEmail("BSZ File Update", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
