#!/bin/python2
# -*- coding: utf-8 -*-
#
# A tool for the automation of tarball downloads from the BSZ.
# Config files for this tool look like this:
"""
[FTP]
host     = vftp.bsz-bw.de
username = swb
password = XXXXXX

[SMTPServer]
server_address  = smtpserv.uni-tuebingen.de
server_user     = qubob16
server_password = XXXXXX

[Kompletter Abzug]
filename_pattern = ^SA-MARC-ixtheo-(\d\d\d\d\d\d).tar.gz$
directory_on_ftp_server = /ixtheo

[Differenzabzug]
filename_pattern = ^(?:TA-MARC-ixtheo|SA-MARC-ixtheo_o|TA-MARC-ixtheo_o)-(\d\d\d\d\d\d).tar.gz$
directory_on_ftp_server = /ixtheo

[Hinweisabzug]
filename_pattern = ^SA-MARC-ixtheo_hinweis-(\d\d\d\d\d\d).tar.gz$
directory_on_ftp_server = /ixtheo

[Loeschlisten]
filename_pattern = ^LOEPPN-(\d\d\d\d\d\d)$
directory_on_ftp_server = /sekkor

[Errors]
filename_pattern = ^Errors_ixtheo_(\d\d\d\d\d\d)$
directory_on_ftp_server = /ixtheo

[Kumulierte Abzuege]
output_directory = /usr/local/ub_tools/bsz_daten_cumulated
"""

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


# Returns "yymmdd_string" incremented by one day unless it equals "000000" (= minus infinity).
def IncrementStringDate(yymmdd_string):
    if yymmdd_string == "000000":
        return yymmdd_string
    date = datetime.datetime.strptime(yymmdd_string, "%y%m%d")
    date = date + datetime.timedelta(days=1)
    return date.strftime("%y%m%d")


def Login(ftp_host, ftp_user, ftp_passwd):
    try:
        ftp = FTP(host=ftp_host, timeout=120)
    except Exception as e:
        util.Error("failed to connect to FTP server! (" + str(e) + ")")

    try:
        ftp.login(user=ftp_user, passwd=ftp_passwd)
    except Exception as e:
        util.Error("failed to login to FTP server! (" + str(e) + ")")
    return ftp


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


# Returns a list of files found in the "directory" directory on an FTP server that match "filename_regex"
# and have a datestamp (YYMMDD) more recent than "download_cutoff_date".
def GetListOfRemoteFiles(ftp, filename_regex, directory, download_cutoff_date):
    try:
        ftp.cwd(directory)
    except Exception as e:
        util.Error("can't change directory to \"" + directory + "\"! (" + str(e) + ")")

    # Retry calling GetMostRecentFile() up to 3 times:
    exception = None
    for i in xrange(3):
        try:
            filename_list = []
            for filename in ftp.nlst():
                 match = filename_regex.match(filename)
                 if match and match.group(1) >= download_cutoff_date:
                     filename_list.append(filename)
            return filename_list
        except Exception as e:
            exception = e
            time.sleep(10 * (i + 1))
    raise exception


# Attempts to retrieve "remote_filename" from an FTP server.
def DownLoadFile(ftp, remote_filename):
    try:
        output = open(remote_filename, "wb")
    except Exception as e:
        util.Error("local open of \"" + remote_filename + "\" failed! (" + str(e) + ")") 
    try:
        def RetrbinaryCallback(chunk):
            try:
                output.write(chunk)
            except Exception as e:
                util.Error("failed to write a data chunk to local file \"" + remote_filename + "\"! (" + str(e) + ")")
        ftp.retrbinary("RETR " + remote_filename, RetrbinaryCallback)
    except Exception as e:
        util.Error("File download failed! (" + str(e) + ")")


# Downloads matching files found in "remote_directory" on the FTP server that have a datestamp
# more recent than "download_cutoff_date".
def DownloadRemoteFiles(ftp, filename_regex, remote_directory, download_cutoff_date):
    filenames = GetListOfRemoteFiles(ftp, filename_regex, remote_directory, download_cutoff_date)
    for filename in filenames:
        DownLoadFile(ftp, filename)
    return filenames


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
            shutil.copy(downloaded_file, output_directory)
    except Exception as e:
        util.Error("Adding file to cumulative collection failed! (" + str(e) + ")")

    return None;


def CumulativeFilenameGenerator(output_directory):
     return os.listdir(output_directory)


def GetBackupDirectoryPath(config):
    try:
        backup_directory = config.get("Kumulierte Abzuege", "output_directory")
    except Exception as e:
        util.Error("could not determine output directory (" + str(e) + ")")

    if not os.path.exists(backup_directory):
        util.Error("backup directory is missing: \"" + backup_directory + "\"!")

    return backup_directory


# We try to keep all differential updates up to and including the last complete data
def CleanUpCumulativeCollection(config):
    backup_directory = GetBackupDirectoryPath(config)

    try:
        filename_pattern_complete_data = config.get("Kompletter Abzug", "filename_pattern")
    except Exception as e:
        util.Error("invalid filename pattern for complete data (" + str(e) + ")")
    try:
        filename_complete_data_regex = re.compile(filename_pattern_complete_data)
    except Exception as e:
        util.Error("filename pattern \"" + filename_pattern_complete_data + "\" failed to compile! ("
                   + str(e) + ")")    

    # Find the latest complete data file
    try:
        most_recent_complete_data_filename = GetMostRecentFile(filename_complete_data_regex,
                                                               CumulativeFilenameGenerator(backup_directory))
    except Exception as e:
        util.Error("Unable to to determine the most recent complete data file (" + str(e) + ")")

    if most_recent_complete_data_filename is None:
        return None

    # Extract the date
    match = filename_complete_data_regex.match(most_recent_complete_data_filename)
    if match and match.group(1):
        most_recent_complete_data_date = match.group(1)
        DeleteAllFilesOlderThan(most_recent_complete_data_date, backup_directory)
    
    return None


# Delete all files that are older than a given date     
def DeleteAllFilesOlderThan(date, directory):
     filename_pattern = '\\D*?-(\\d{6}).*'
     try:
         filename_regex = re.compile(filename_pattern)
     except Exception as e:
           util.Error("File name pattern \"" + filename_to_delete_pattern + "\" failed to compile! (" + str(e) + ")")

     for filename in CumulativeFilenameGenerator(directory):
         match = filename_regex.match(filename)
         if match and match.group(1) < date:
            os.remove(directory + "/" +  match.group())

     return None


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


def DownloadData(config, section, ftp, download_cutoff_date, msg):
    try:
        filename_pattern = config.get(section, "filename_pattern")
        directory_on_ftp_server = config.get(section, "directory_on_ftp_server")
    except Exception as e:
        util.Error("Invalid section \"" + section + "\" in config file! (" + str(e) + ")")

    try:
        filename_regex = re.compile(filename_pattern)
    except Exception as e:
        util.Error("File name pattern \"" + filename_pattern + "\" failed to compile! (" + str(e) + ")")

    downloaded_files = DownloadRemoteFiles(ftp, filename_regex, directory_on_ftp_server, download_cutoff_date)
    if len(downloaded_files) == 0:
        msg.append("No more recent file for pattern \"" + filename_pattern + "\"!\n")
    else:
        msg.append("Successfully downloaded:\n" + string.join(downloaded_files, '\n') + '\n')
        AddToCumulativeCollection(downloaded_files, config)
    return downloaded_files


def DownloadCompleteData(config, ftp, download_cutoff_date, msg):
    downloaded_files = DownloadData(config, "Kompletter Abzug", ftp, download_cutoff_date, msg)
    if len(downloaded_files) == 1:
        return downloaded_files[0]
    elif len(downloaded_files) == 0:
        return None
    else:
        util.Error("downloaded multiple complete date tar files!")


def DownloadOtherData(config, section, ftp, download_cutoff_date, msg):
    DownloadData(config, section, ftp, download_cutoff_date, msg)


def Main():
    if len(sys.argv) != 2:
        util.SendEmail(os.path.basename(sys.argv[0]),
                       "This script needs to be called with an email address as the only argument!\n", priority=1)
        sys.exit(-1)
    util.default_email_recipient = sys.argv[1]
    try:
        config = util.LoadConfigFile()
        ftp_host   = config.get("FTP", "host")
        ftp_user   = config.get("FTP", "username")
        ftp_passwd = config.get("FTP", "password")
    except Exception as e:
        util.Error("failed to read config file! ("+ str(e) + ")")

    ftp = Login(ftp_host, ftp_user, ftp_passwd)
    msg = []

    download_cutoff_date = IncrementStringDate(GetCutoffDateForDownloads(config))
    complete_data_filename = DownloadCompleteData(config, ftp, download_cutoff_date, msg)
    if complete_data_filename is not None:
        download_cutoff_date = ExtractDateFromFilename(complete_data_filename)
    DownloadOtherData(config, "Differenzabzug", ftp, download_cutoff_date, msg)
    DownloadOtherData(config, "Loeschlisten", ftp, download_cutoff_date, msg)
    if config.has_section("Hinweisabzug"):
        DownloadOtherData(config, "Hinweisabzug", ftp, 000000, msg)
    if config.has_section("Errors"):
        DownloadOtherData(config, "Errors", ftp, download_cutoff_date, msg)
    CleanUpCumulativeCollection(config)
    util.SendEmail("BSZ File Update", string.join(msg, ""), priority=5)


try:
    Main()
except Exception as e:
    util.SendEmail("BSZ File Update", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
