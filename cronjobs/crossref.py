#!/bin/python2
# -*- coding: utf-8 -*-
#
# A tool for the automation of tarball downloads from the BSZ.
# Config files for this tool look like this:
"""
[FTP]
host     = vftp.bsz-bw.de
username = transfer
password = XXXXXX

[SMTPServer]
server_address  = smtpserv.uni-tuebingen.de
server_user     = qubob16
server_password = XXXXXX
"""

from ftplib import FTP
import os
import process_util
import traceback
import util


def CreateLogFileName():
    return util.MakeLogFileName(os.path.basename(__file__), util.GetLogDirectory())


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
        util.Error("failed to read config file! (" + str(e) + ")")

    # Download data from Crossref:
    log_file_name = CreateLogFileName()
    crossref_xml_file = "/tmp/crossref.xml"
    os.unlink(crossref_xml_file)
    util.ExecOrDie("/usr/local/bin/crossref_downloader", [ crossref_xml_file ], log_file_name)

    # Upload the XML data to the BSZ FTP server:
    ftp = util.FTPLogin(ftp_host, ftp_user, ftp_passwd)
    try:
        with open(crossref_xml_file, "rb") as xml_file:
            ftp.storbinary("STOR crossref.xml", xml_file)
    except Exception as e:
        util.Error("failed to read config file! (" + str(e) + ")")
    os.unlink(crossref_xml_file)
    
    util.SendEmail("Crossref Data Import",
                   "Successfully imported Crossref data and uploaded it to the BSZ FTP server.", priority=5)


try:
    Main()
except Exception as e:
    util.SendEmail("Crossref Data Import", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
