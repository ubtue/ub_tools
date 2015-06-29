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

[Misc]
filename_pattern = WA-MARC-krimdok-(\d\d\d\d\d\d).tar.gz
"""


from email.mime.text import MIMEText
from ftplib import FTP
import configparser
import os
import re
import smtplib
import sys


def SendEmail(subject, msg, sender="fetch_marc_updates.py", recipient="johannes.ruscheinski@uni-tuebingen.de"):
    config = LoadConfigFile(sys.argv[0][:-2] + "conf")
    try:
        server_address  = config["SMTPServer"]["server_address"]
        server_user     = config["SMTPServer"]["server_user"]
        server_password = config["SMTPServer"]["server_password"]
    except Exception as e:
        print("failed to read config file! ("+ str(e) + ")", file=sys.stderr)
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
        Warning("Failed to send your email: " + str(e))
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


def GetMostRecentTarball(filename_regex, filename_generator):
    most_recent_date = "000000"
    most_recent_tarball = None
    for filename in filename_generator:
         match = filename_regex.match(filename)
         if match and match.group(1) > most_recent_date:
             most_recent_date = match.group(1)
             most_recent_tarball = filename
    return most_recent_tarball


def GetMostRecentLocalTarball(filename_regex):
    def LocalFilenameGenerator():
        return os.listdir(".")

    return GetMostRecentTarball(filename_regex, LocalFilenameGenerator())


def GetMostRecentRemoteTarball(ftp, filename_regex):
    try:
        ftp.cwd("001") # UB Tübingen
    except Exception as e:
        Error("can't change directory to \"001\"! (" + str(e) + ")")

    return GetMostRecentTarball(filename_regex, ftp.nlst())


# Compares remote and local filenames against pattern and, if the remote filename
# is more recent than the local one, downloads it.
def DownloadMoreRecentTarball(ftp, filename_regex):
    most_recent_remote_tarball = GetMostRecentRemoteTarball(ftp, filename_regex)
    if most_recent_remote_tarball is None:
        Error("No filename matched \"" + filename_pattern + "\"!")
    print("Found recent remote tarball:", most_recent_remote_tarball)
    most_recent_local_tarball = GetMostRecentLocalTarball(filename_regex)
    if most_recent_local_tarball is not None:
        print("Found recent local tarball:", most_recent_local_tarball)
    if (most_recent_local_tarball is None) or (most_recent_remote_tarball > most_recent_local_tarball):
        try:
            output = open(most_recent_remote_tarball, "wb")
        except Exception as e:
            Error("local open of \"" + most_recent_remote_tarball + "\" failed! (" + str(e) + ")") 
        try:
            def RetrbinaryCallback(chunk):
                try:
                    output.write(chunk)
                except Exception as e:
                    Error("failed to write a data chunk to local file \"" + most_recent_remote_tarball + "\"! ("
                          + str(e) + ")")
            ftp.retrbinary("RETR " + most_recent_remote_tarball, RetrbinaryCallback)
        except Exception as e:
            Error("File download failed! (" + str(e) + ")")
        SendEmail("BSZ Tarball Update", "Successfully downloaded \"" + most_recent_remote_tarball + "\"!")
    else:
        SendEmail("No new BSZ Tarball", "Most recent remote file is no more recent than most recent local file!")


def Main():
    config = LoadConfigFile(sys.argv[0][:-2] + "conf")
    try:
        ftp_host         = config["FTP"]["host"]
        ftp_user         =  config["FTP"]["username"]
        ftp_passwd       = config["FTP"]["password"]
        filename_pattern = config["Misc"]["filename_pattern"]
    except Exception as e:
        Error("failed to read config file! ("+ str(e) + ")")

    try:
        filename_regex = re.compile(filename_pattern)
    except Exception as e:
        Error("File name pattern \"" + filename_pattern + "\" failed to compile! (" + str(e) + ")")

    ftp = Login(ftp_host, ftp_user, ftp_passwd)
    DownloadMoreRecentTarball(ftp, filename_regex)


Main()
