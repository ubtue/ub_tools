#!/usr/bin/python3
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
"""


from email.mime.text import MIMEText
from ftplib import FTP
import configparser
import os
import re
import smtplib
import sys


def SendEmail(subject, msg, sender="fetch_marc_updates@ub.uni-tuebingen.de",
              recipient="johannes.ruscheinski@uni-tuebingen.de"):
    config = LoadConfigFile(sys.argv[0][:-2] + "conf")
    try:
        server_address  = config["SMTPServer"]["server_address"]
        server_user     = config["SMTPServer"]["server_user"]
        server_password = config["SMTPServer"]["server_password"]
    except Exception as e:
        print("failed to read config file! (" + str(e) + ")", file=sys.stderr)
        sys.exit(-1)

    message = MIMEText(msg)
    message["Subject"] = subject
    message["From"] = sender
    message["To"] = recipient
    server = smtplib.SMTP(server_address)
    try:
        server.ehlo()
        server.starttls()
        server.login(server_user, server_password)
        server.sendmail(sender, [recipient], message.as_string())
    except Exception as e:
        print("Failed to send your email: " + str(e), file=sys.stderr)
        sys.exit(-1)
    server.quit()


def Error(msg):
    print(sys.argv[0] + ": " + msg, file=sys.stderr)
    SendEmail("SWB FTP Failed!", msg)
    sys.exit(1)


def Login(ftp_host, ftp_user, ftp_passwd):
    try:
        ftp = FTP(host=ftp_host)
    except Exception as e:
        Error("failed to connect to FTP server! (" + str(e) + ")")

    try:
        ftp.login(user=ftp_user, passwd=ftp_passwd)
    except Exception as e:
        Error("failed to login to FTP server! (" + str(e) + ")")
    return ftp


def LoadConfigFile(path):
    try:
        if not os.access(path, os.R_OK):
            Error("can't open \"" + path + "\" for reading!")
        config = configparser.ConfigParser()
        config.read(path)
        return config
    except Exception as e:
        Error("failed to load the config file from \"" + path + "\"! (" + str(e) + ")")


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
        Error("can't change directory to \"" + directory + "\"! (" + str(e) + ")")

    return GetMostRecentFile(filename_regex, ftp.nlst())


# Compares remote and local filenames against pattern and, if the remote filename
# is more recent than the local one, downloads it.
def DownloadMoreRecentFile(ftp, filename_regex, remote_directory):
    most_recent_remote_file = GetMostRecentRemoteFile(ftp, filename_regex, remote_directory)
    if most_recent_remote_file is None:
        Error("No filename matched \"" + filename_pattern + "\"!")
    print("Found recent remote file:", most_recent_remote_file)
    most_recent_local_file = GetMostRecentLocalFile(filename_regex)
    if most_recent_local_file is not None:
        print("Found recent local file:", most_recent_local_file)
    if (most_recent_local_file is None) or (most_recent_remote_file > most_recent_local_file):
        try:
            output = open(most_recent_remote_file, "wb")
        except Exception as e:
            Error("local open of \"" + most_recent_remote_file + "\" failed! (" + str(e) + ")") 
        try:
            def RetrbinaryCallback(chunk):
                try:
                    output.write(chunk)
                except Exception as e:
                    Error("failed to write a data chunk to local file \"" + most_recent_remote_file + "\"! ("
                          + str(e) + ")")
            ftp.retrbinary("RETR " + most_recent_remote_file, RetrbinaryCallback)
        except Exception as e:
            Error("File download failed! (" + str(e) + ")")
        return most_recent_remote_file
    else:
        return None


def Main():
    config = LoadConfigFile(sys.argv[0][:-2] + "conf")
    try:
        ftp_host   = config["FTP"]["host"]
        ftp_user   = config["FTP"]["username"]
        ftp_passwd = config["FTP"]["password"]
    except Exception as e:
        Error("failed to read config file! ("+ str(e) + ")")

    ftp = Login(ftp_host, ftp_user, ftp_passwd)
    msg = ""
    for section in config.sections():
        if section == "FTP" or section == "SMTPServer":
            continue

        print("Processing section " + section)
        try:
            filename_pattern = config[section]["filename_pattern"]
            directory_on_ftp_server = config[section]["directory_on_ftp_server"]
        except Exception as e:
            Error("Invalid section \"" + section + "\" in config file! (" + str(e) + ")")

        try:
            filename_regex = re.compile(filename_pattern)
        except Exception as e:
            Error("File name pattern \"" + filename_pattern + "\" failed to compile! (" + str(e) + ")")

        downloaded_file = DownloadMoreRecentFile(ftp, filename_regex, directory_on_ftp_server)
        if downloaded_file is None:
            msg += "No more recent file for pattern \"" + filename_pattern + "\"!"
        else:
            msg += "Successfully downloaded \"" + downloaded_file + "\"."
    print("About to send email...")
    SendEmail("BSZ File Update", msg)


try:
    Main()
except Exception as e:
    SendEmail("BSZ File Update", "An unexpected error occurred: " + str(e))
