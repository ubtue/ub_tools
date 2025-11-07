#!/bin/python3
# -*- coding: utf-8 -*-
import sys
import util


def Main():
    if len(sys.argv) != 3:
        util.Info("usage: " + sys.argv[0] + " section entry", file=sys.stderr)
        sys.exit(-1)

    config = util.LoadConfigFile()
    util.Info(config.get(sys.argv[1], sys.argv[2]))


Main()
