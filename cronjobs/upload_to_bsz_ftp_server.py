#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# A tool to upload files to the BSZ FTP server.
# In earlier times we had a bash script for this which was using the "ftp" command,
# but due to problems with error handling granularity we switched to python.
import bsz_util
import os
import sys
import traceback
import util


def Main():
    if len(sys.argv) != 3:
        util.Error("Usage: " + sys.argv[0] + " local_file remote_folder_path")

    local_file_path = sys.argv[1]
    remote_folder_path = sys.argv[2]
    remote_file_name = os.path.basename(local_file_path)
    remote_file_name_tmp = remote_file_name + ".tmp"

    ftp = bsz_util.GetFTPConnection("SFTP_Upload")
    ftp.changeDirectory(remote_folder_path)
    ftp.uploadFile(local_file_path, remote_file_name_tmp)
    ftp.renameFile(remote_file_name_tmp, remote_file_name)


try:
    Main()
except Exception as e:
    util.Error(str(e) + "\n\n" + traceback.format_exc(20))