# Python 3 module
# -*- coding: utf-8 -*-

from sftp_connection import SFTPConnection
import os
import re
import time
import util


def FoundNewBSZDataFile(link_filename):
    try:
        statinfo = os.stat(link_filename)
        file_creation_time = statinfo.st_ctime
    except OSError:
        util.Error("Symlink \"" + link_filename + "\" is missing or dangling!")
    old_timestamp = util.ReadTimestamp()
    return old_timestamp < file_creation_time


def GetFTPConnection(credential_type=None):
    ftp_host   = ""
    ftp_user   = ""
    ftp_passwd = ""
    try:
        bsz_config = util.LoadConfigFile(util.default_config_file_dir + "BSZ.conf")
        if credential_type != None :
            ftp_host   = bsz_config.get(credential_type, "host")
            ftp_user   = bsz_config.get(credential_type, "username")
            ftp_passwd = bsz_config.get(credential_type, "password")
        else:
            ftp_host   = bsz_config.get("SFTP_Download", "host")
            ftp_user   = bsz_config.get("SFTP_Download", "username")
            ftp_passwd = bsz_config.get("SFTP_Download", "password")
    except Exception as e:
        util.Error("failed to read config file! (" + str(e) + ")")

    return SFTPConnection(ftp_host, ftp_user, ftp_passwd)


# Returns a list of files found in the "directory" directory on an FTP server that match "filename_regex"
# and have a datestamp (YYMMDD) more recent than "download_cutoff_date".
def GetListOfRemoteFiles(ftp, filename_regex, directory, download_cutoff_date):
    try:
        ftp.changeDirectory(directory)
    except Exception as e:
        util.Error("can't change directory to \"" + directory + "\"! (" + str(e) + ")")

    # Retry calling GetMostRecentFile() up to 3 times:
    exception = None
    for attempt_number in range(3):
        try:
            filename_list = []
            for filename in ftp.listDirectory():
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
    return len(matching_remote_files) % 2 == 0


# Downloads matching files found in "remote_directory" on the FTP server that have a datestamp
# more recent than "download_cutoff_date" if some consistency check succeeds
def DownloadRemoteFiles(config, ftp, filename_regex, remote_directory, download_cutoff_date):
    filenames = GetListOfRemoteFiles(ftp, filename_regex, remote_directory, download_cutoff_date)
    if NeedsBothInstances(filename_regex):
        if not AreBothInstancesPresent(filename_regex, filenames):
            util.Error("Skip downloading since apparently generation of the files on the FTP server is not complete!")
    for filename in filenames:
        ftp.downloadFile(filename, filename)
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


def GetFTPDirectoryForSection(config, section):
    try:
        directory_on_ftp_server = config.get(section, "directory_on_ftp_server")
    except Exception as e:
        util.Error("Invalid section \"" + section + "\" in config file! (" + str(e) + ")")
    return directory_on_ftp_server
