#!/bin/python2
# -*- coding: utf-8 -*-
"""
Given an absolut path for the config file, that will be used.  If it is a relative path, the first location
searched will be /usr/local/var/lib/tuelib/$hostname/$config_file.  If that is not readable, next
/usr/local/var/lib/tuelib/$config_file will be tried.  If none of those work, the program will exit with an
error message.
"""


import ConfigParser
import os
import socket
import sys
import util


DEFAULT_CONFIG_FILE_LOCATION = "/usr/local/var/lib/tuelib/cronjobs/"


def Main():
    if len(sys.argv) != 4:
        util.Info("usage: " + sys.argv[0] + " config_file section entry", file=sys.stderr)
        sys.exit(-1)

    config_file = sys.argv[1]
    if not config_file.startswith("/"):
        if os.access(DEFAULT_CONFIG_FILE_LOCATION + socket.gethostname() + "/", os.R_OK):
            config_file = DEFAULT_CONFIG_FILE_LOCATION + socket.gethostname() + "/" + config_file
        else:
            config_file = DEFAULT_CONFIG_FILE_LOCATION + config_file
    if not os.access(config_file, os.R_OK):
        util.Info(sys.argv[0] + ": can't read \"" + config_file + "\"!", file=sys.stderr)
        sys.exit(-1)

    config = ConfigParser.ConfigParser()
    config.read(config_file)
    util.Info(config.get(sys.argv[2], sys.argv[3]))


Main()
