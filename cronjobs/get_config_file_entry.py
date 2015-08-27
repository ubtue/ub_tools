#!/bin/python2
# -*- coding: utf-8 -*-


from __future__ import print_function
import ConfigParser
import sys


def Main():
    if len(sys.argv) != 4:
        print("usage: " + sys.argv[0] + " config_file section entry", file=sys.stderr)
        sys.exit(-1)

    config = ConfigParser.ConfigParser()
    config.read(sys.argv[1])
    print(config.get(sys.argv[2], sys.argv[3]))


Main()
