#!/bin/python2
# -*- coding: utf-8 -*-
#
# Downloads metadata from Crossref.org, generates MARC-XML from those data and uploads the MARC-XML to the BSZ.
"""
[FTP]
host     = vftp.bsz-bw.de
username = swb
password = XXXXXX

[SMTPServer]
server_address  = smtpserv.uni-tuebingen.de
server_user     = qubob16
server_password = XXXXXX

[Upload]
directory_on_ftp_server = "/Tuebingen_crossref"
"""

from ftplib import FTP
import datetime
import os
import re
import subprocess
import sys
import time
import traceback
import util


# Attempts to upload "local_filename" to an FTP server.
def UploadFile(ftp, local_filename, remote_filename):
    try:
        output = open(local_filename, "rb")
    except Exception as e:
        util.Error("local open of \"" + local_filename + "\" failed! (" + str(e) + ")")
    try:
        ftp.storbinary("STOR " + remote_filename)
    except Exception as e:
        util.Error("File upload failed! (" + str(e) + ")")


# Downloads metadata from Crossref.org and writes as MARC-XML to "output_marc_filename:.
# @return The number of records that were written.
def DownloadCrossrefData(output_marc_filename):
    util.ExecOrDie("/usr/local/bin/crossref_downloader", "/var/lib/tuelib/crossref_journal_list",
                   output_marc_filename)
    process = subprocess.Popen(["marc_size", output_marc_filename], stdout=subprocess.PIPE)
    size = process.stdout.readline()
    subprocess.Pclose(process)
    return int(size)


def GenerateRemoteFilename():
    return "ub-tue-crossref-" + time.strftime("%Y-%m-%d") + ".xml"


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

    marc_filename = "/tmp/crossref_marc.xml"
    no_of_records = DownloadCrossrefData(marc_filename)
    os.unlink(marc_filename)
    if no_of_records == 0:
        email_msg_body = "No new records.\n\n"
    else:
        ftp = util.FTPLogin(ftp_host, ftp_user, ftp_passwd)
        UploadFile(ftp, marc_filename, GenerateRemoteFilename())
        email_msg_body = "Uploaded " + str(size) + " MARC records to the BSZ FTP server.\n\n"
    util.SendEmail("BSZ Crossref File Upload", email_msg_body, priority=5)


try:
    Main()
except Exception as e:
    util.SendEmail("BSZ Crossref File Upload", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
