#!/bin/python2
# -*- coding: utf-8 -*-
# Transfers uploaded fulltexts to the fulltext server
"""
[SFTP]
host     = nu.ub.uni-tuebingen.de
username = sftpuser
keyfile  = XXXXXX

[SMTPServer]
server_address  = smtpserv.uni-tuebingen.de
server_user     = qubob16
server_password = XXXXXX

[Upload]
local_directory = "/usr/local/webdav/"
directory_on_sftp_server = "incoming"
"""

import datetime
import fnmatch
import functools
import os
import paramiko
import re
import subprocess
import sys
import time
import traceback
import util


# see https://stackoverflow.com/questions/1724693/find-a-file-in-python (180704)
def FindFilesByPattern(pattern, path):
    result = []
    for root, dirs, files in os.walk(path):
        for name in files:
            if fnmatch.fnmatch(name, pattern):
                result.append(os.path.join(root, name))
    return result


def StripPathPrefix(entry, start):
    return os.path.relpath(entry, start)

def ExtractDirectories(entry):
    return os.path.dirname(entry)


def GetFulltextDirectoriesToTransfer(local_top_dir):
    os.chdir(local_top_dir)
    full_paths =  FindFilesByPattern('*.7z', local_top_dir)
    stripped_paths = list(map(lambda path:StripPathPrefix(path, local_top_dir), full_paths))
    directory_set = set(list(map(ExtractDirectories, stripped_paths)))
    return directory_set
    

#def TransferFiles(sftp, local_filename, remote_filename):

def Main():
    if len(sys.argv) != 2:
        util.SendEmail(os.path.basename(sys.argv[0]),
                       "This script needs to be called with an email address as the only argument!\n", priority=1)
        sys.exit(-1)
    util.default_email_recipient = sys.argv[1]
    try:
        config = util.LoadConfigFile()
        sftp_host                = config.get("SFTP", "host")
        sftp_user                = config.get("SFTP", "username")
        sftp_keyfile             = config.get("SFTP", "keyfile")
        local_directory          = config.get("Upload", "local_directory")
        directory_on_sftp_server = config.get("Upload", "directory_on_sftp_server")
        
    except Exception as e:
        util.Error("failed to read config file! (" + str(e) + ")")
    # Check directories with new Data
    os.chdir(local_directory)
    for filename in GetFulltextDirectoriesToTransfer(local_directory):
        print filename

    # Transfer the data
    util.ExecOrDie("/usr/local/bin/transfer_fulltext.sh", 
                   [sftp_host, sftp_user, sftp_keyfile, local_directory, directory_on_sftp_server])


try:
    Main()
except Exception as e:
    print("Transfer Fulltexts", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20))

