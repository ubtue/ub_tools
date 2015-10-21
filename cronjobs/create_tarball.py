#!/bin/python2
# -*- coding: utf-8 -*-
#
# Test harness for util.CreateTarball().

from __future__ import print_function
import sys
import util


def Usage():
    print("usage: " + sys.argv[0] + " archive_name file_name1:member_name1 "
          + "[file_name2:member_name2 .. file_nameN:member_nameN]")
    print("       The colons and member names can be left out in which case the files will be stored under")
    print("       their original names.")
    sys.exit(-1)


def Main():
    util.default_email_sender = "create_tarball@ub.uni-tuebingen.de"
    if len(sys.argv) < 2:
        Usage()

    arg_no = 2
    file_and_member_name_list = []
    while arg_no < len(sys.argv):
        first_colon_pos = (sys.argv[arg_no]).find(":")
        if first_colon_pos == -1:
            file_and_member_name_list.append((sys.argv[arg_no], None))
        else:
            file_and_member_name_list.append((sys.argv[arg_no][0 : first_colon_pos],
                                              sys.argv[arg_no][first_colon_pos + 1 : ]))
        arg_no += 1

    util.CreateTarball(sys.argv[1], file_and_member_name_list)


Main()
