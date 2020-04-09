# Python 3 module
# -*- coding: utf-8 -*-
import os
import sys
import util

def FoundNewBSZDataFile(link_filename):
    try:
        statinfo = os.stat(link_filename)
        file_creation_time = statinfo.st_ctime
    except OSError as e:
        util.Error("in FoundNewBSZDataFile: Symlink \"" + link_filename + "\" is missing or dangling!")
    old_timestamp = util.ReadTimestamp()
    return old_timestamp < file_creation_time

