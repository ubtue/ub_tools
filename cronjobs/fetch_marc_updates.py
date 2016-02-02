#!/bin/python2
# -*- coding: utf-8 -*-
#
# A tool for the automation of tarball downloads from the BSZ.
# Config files for this tool look like this:
"""
[FTP]
host     = vftp.bsz-bw.de
username = swb
password = XXXXXXX

[SMTPServer]
server_address  = smtpserv.uni-tuebingen.de
server_user     = XXXXXX
server_password = XXXXXX

[Kompletter Abzug]
filename_pattern = WA-MARC-krimdok-(\d\d\d\d\d\d).tar.gz
directory_on_ftp_server = /001

[Loeschlisten]
filename_pattern = LOEPPN-(\d\d\d\d\d\d)
directory_on_ftp_server = /sekkor

[Kumulierte Abzuege]
output_directory = /tmp
"""

from ftplib import FTP
import os
import re
import sys
import time
import traceback
import util
import shutil

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


def GetMostRecentLocalFile(filename_regex):
    def LocalFilenameGenerator():
        return os.listdir(".")

    return GetMostRecentFile(filename_regex, LocalFilenameGenerator())


def GetMostRecentRemoteFile(ftp, filename_regex, directory):
    try:
        ftp.cwd(directory)
    except Exception as e:
        util.Error("can't change directory to \"" + directory + "\"! (" + str(e) + ")")

    # Retry calling GetMostRecentFile() up to 3 times:
    exception = None
    for i in xrange(3):
        try:
            return GetMostRecentFile(filename_regex, ftp.nlst())
        except Exception as e:
            exception = e
            time.sleep(10 * (i + 1))
    raise exception


# Compares remote and local filenames against pattern and, if the remote filename
# is more recent than the local one, downloads it.
def DownloadMoreRecentFile(ftp, filename_regex, remote_directory):
    most_recent_remote_file = GetMostRecentRemoteFile(ftp, filename_regex, remote_directory)
    if most_recent_remote_file is None:
        return None
    util.Info("Found recent remote file:", most_recent_remote_file)
    most_recent_local_file = GetMostRecentLocalFile(filename_regex)
    if most_recent_local_file is not None:
        util.Info("Found recent local file:", most_recent_local_file)
    if (most_recent_local_file is None) or (most_recent_remote_file > most_recent_local_file):
        try:
            output = open(most_recent_remote_file, "wb")
        except Exception as e:
            util.Error("local open of \"" + most_recent_remote_file + "\" failed! (" + str(e) + ")") 
        try:
            def RetrbinaryCallback(chunk):
                try:
                    output.write(chunk)
                except Exception as e:
                    util.Error("failed to write a data chunk to local file \"" + most_recent_remote_file + "\"! ("
                               + str(e) + ")")
            ftp.retrbinary("RETR " + most_recent_remote_file, RetrbinaryCallback)
        except Exception as e:
            util.Error("File download failed! (" + str(e) + ")")
        util.SafeSymlink(most_recent_remote_file, re.sub("\\d\\d\\d\\d\\d\\d", "current", most_recent_remote_file))
        return most_recent_remote_file
    else:
        return None

# Cumulatively saves downloaded data to an external location to have complete trace of 
# downloaded data. Thus, the complete data should be reconstructible  
def AddToCumulativeCollection(downloaded_file, config):
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
        shutil.copy(downloaded_file, output_directory)
    except Exception as e:
        util.Error("Adding file to cumulative collection failed! (" + str(e) + ")")

    return None;

def CumulativeFilenameGenerator(output_directory):
     return os.listdir(output_directory)

# We try to keep all differential updates up to and including the last complete data

def CleanUpCumulativeCollection(config):
    # Check config   
 
    try:
        directory =  config.get("Kumulierte Abzuege", "output_directory")
    except Exception as e:
        util.Error("Could not determine output directory (" + str(e) + ")")

    # We are done if there is not even the directory    
    if not os.path.exists(directory):
        return None    
 
    try:
        filename_pattern_complete_data = config.get("Kompletter Abzug", "filename_pattern")
    except Exception as e:
        util.Error("Invalid filename pattern for complete data (" + str(e) + ")")
    try:
        filename_complete_data_regex = re.compile(filename_pattern_complete_data)
    except Exception as e:
         util.Error("File name pattern \"" + filename_pattern_complete_data + "\" failed to compile! (" + str(e) + ")")    

    # Find the latest complete data file
    try:
        most_recent_complete_data_filename = GetMostRecentFile(filename_complete_data_regex, CumulativeFilenameGenerator(directory))
    except Exception as e:
        util.Error("Unable to to determine the most recent complete data file (" + str(e) + ")")

    if most_recent_complete_data_filename is None:
        return None

    # Extract the date
    match = filename_complete_data_regex.match(most_recent_complete_data_filename)
    if match and match.group(1):
        most_recent_complete_data_date = match.group(1)
        DeleteAllFilesOlderThan(most_recent_complete_data_date, directory)
    
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


def Main():
    util.default_email_sender = "fetch_marc_updates@ub.uni-tuebingen.de"
    if len(sys.argv) != 2:
         util.SendEmail(os.path.basename(sys.argv[0]),
                        "This script needs to be called with an email address as the only argument!\n")
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
    msg = ""
    for section in config.sections():
        if section == "FTP" or section == "SMTPServer" or section == "Kumulierte Abzuege":
            continue

        util.Info("Processing section " + section)
        try:
            filename_pattern = config.get(section, "filename_pattern")
            directory_on_ftp_server = config.get(section, "directory_on_ftp_server")
        except Exception as e:
            util.Error("Invalid section \"" + section + "\" in config file! (" + str(e) + ")")

        try:
            filename_regex = re.compile(filename_pattern)
        except Exception as e:
            util.Error("File name pattern \"" + filename_pattern + "\" failed to compile! (" + str(e) + ")")

        downloaded_file = DownloadMoreRecentFile(ftp, filename_regex, directory_on_ftp_server)
        if downloaded_file is None:
            msg += "No more recent file for pattern \"" + filename_pattern + "\"!\n"
        else:
            msg += "Successfully downloaded \"" + downloaded_file + "\".\n"
            AddToCumulativeCollection(downloaded_file, config)
    CleanUpCumulativeCollection(config)
    util.SendEmail("BSZ File Update", msg)


try:
    Main()
except Exception as e:
    util.SendEmail("BSZ File Update", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20))
