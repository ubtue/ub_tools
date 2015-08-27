#!/bin/python2
# -*- coding: utf-8 -*-
"""
Given an absolut path for the config file, that will be used.  If it is a relative path, the first location
searched will be /var/lib/tuelib/$hostname/$config_file.  If that is not readable, next
/var/lib/tuelib/$config_file will be tried.  If none of those work, the program will exit with an
error message.
"""


from __future__ import print_function
import ConfigParser
import os
import socket
import sys


def Main():
    if len(sys.argv) != 4:
        print("usage: " + sys.argv[0] + " config_file section entry", file=sys.stderr)
        sys.exit(-1)

    config_file = sys.argv[1]
    if not config_file.startswith("/"):
        if os.access("/var/lib/tuelib/" + socket.gethostname() + "/", os.R_OK):
            config_file = "/var/lib/tuelib/" + socket.gethostname() + "/" + config_file
        else:
            config_file = "/var/lib/tuelib/" + config_file
    if not os.access(config_file, os.R_OK):
        print(sys.argv[0] + ": can't read \"" + config_file + "\"!", file=sys.stderr)
        sys.exit(-1)

    config = ConfigParser.ConfigParser()
    config.read(config_file)
    print(config.get(sys.argv[2], sys.argv[3]))


Main()
