#!/bin/python3
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
directory_on_ftp_server = "Tuebingen_crossref2"
"""

from ftp_connection import FTPConnection
import datetime
import os
import re
import subprocess
import sys
import time
import traceback
import util


# Downloads metadata from Crossref.org and writes as MARC-XML to "output_marc_filename:.
# @return The number of records that were written.
def DownloadCrossrefData(output_marc_filename):
    util.ExecOrDie("/usr/local/bin/crossref_downloader",
                   [ "/usr/local/var/lib/tuelib/crossref_downloader/crossref_journal_list", output_marc_filename ],
                   "/proc/self/fd/1")
    process = subprocess.Popen(["marc_size", output_marc_filename], stdout=subprocess.PIPE)
    size = process.stdout.readline()
    return int(size) if len(size) > 0 else 0


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
        ftp_host                = config.get("FTP", "host")
        ftp_user                = config.get("FTP", "username")
        ftp_passwd              = config.get("FTP", "password")
        directory_on_ftp_server = config.get("Upload", "directory_on_ftp_server")
    except Exception as e:
        util.Error("failed to read config file! (" + str(e) + ")")

    marc_filename = "/tmp/crossref_marc.xml"
    no_of_records = DownloadCrossrefData(marc_filename)
    if no_of_records == 0:
        email_msg_body = "No new records.\n\n"
    else:
        ftp = FTPConnection(ftp_host, ftp_user, ftp_passwd)
        ftp.changeDirectory(directory_on_ftp_server)
        ftp.uploadFile(marc_filename, GenerateRemoteFilename())
        email_msg_body = "Uploaded " + str(no_of_records) + " MARC records to the BSZ FTP server.\n\n"
    os.unlink(marc_filename)
    util.SendEmail("BSZ Crossref File Upload", email_msg_body, priority=5)


try:
    Main()
except Exception as e:
    util.SendEmail("BSZ Crossref File Upload", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
