#!/bin/python3
# Python 3 module
# -*- coding: utf-8 -*-



import sys
import util


if len(sys.argv) != 2:
    print("usage: " + sys.argv[0] + " glob", file=sys.stderr)
    sys.exit(-1)

most_recent_matching_name = util.getMostRecentFileMatchingGlob(sys.argv[1])
if most_recent_matching_name is None:
    print("*no match found*")
else:
    print(most_recent_matching_name)
