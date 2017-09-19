#!/bin/python2
# -*- coding: utf-8 -*-
# Test program for testing the process_util.Exec function's ability to append stdout to an existing file.


import process_util


process_util.Exec("/bin/ls", ["-1"], new_stdout="/tmp/test_stdout", append_stdout=True)
