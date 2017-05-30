#!/bin/python2
# -*- coding: utf-8 -*-
#
# A tool for the automatic generation of a Beacon file.
# (https://meta.wikimedia.org/wiki/Dynamic_links_to_external_resources)

import datetime
import os
import re
import sys
import traceback
import util


def GetMostRecentBSZFile(filename_pattern):
    def FilenameGenerator():
        if local_directory is None:
            return os.listdir("/usr/local/ub_tools/bsz_daten/")
        else:
            return os.listdir(local_directory)

    try:
        filename_regex = re.compile(filename_pattern_complete_data)
    except Exception as e:
        util.Error("filename pattern \"" + filename_pattern + "\" failed to compile! (" + str(e) + ")")
    most_recent_date = "000000"
    most_recent_file = None
    for filename in filename_generator:
        match = filename_regex.match(filename)
        if match and match.group(1) > most_recent_date:
            most_recent_date = match.group(1)
            most_recent_file = filename
    return None if most_recent_file is None else "/usr/local/ub_tools/bsz_daten/" + most_recent_file


def Main():
    if len(sys.argv) != 4:
        util.SendEmail(os.path.basename(sys.argv[0]),
                       "This script needs to be called with an email address, the beacon header file and an "
                       "output path as arguments!\n", priority=1)
        sys.exit(-1)
    util.default_email_recipient = sys.argv[1]

    most_recent_authority_filename = GetMostRecentBSZFile("^Normdaten-(\d\d\d\d\d\d).mrc$")
    if most_recent_authority_filename is None:
        util.SendEmail("Beacon Generator", "Found no matching authority files!", priority=1)

    most_recent_titles_filename = GetMostRecentBSZFile("^GesamtTiteldaten-(\d\d\d\d\d\d).mrc$")
    if most_recent_titles_filename is None:
        util.SendEmail("Beacon Generator", "Found no matching title files!", priority=1)

    # Extract the GND numbers from the 035$a subfield of the MARC authority data:
    _035a_contents_filename = "/tmp/035a"
    gnd_numbers_path = "/tmp/gnd_numbers"
    util.ExecOrDie("/usr/local/bin/marc_grep", [ most_recent_authority_filename, "\"035a\"", "no_label" ],
                   _035a_contents_filename)
    util.ExecOrDie("/bin/egrep", [ "^\\(DE-588\\)", _035a_contents_filename ], gnd_numbers_path)

    # Count GND references in the title data:
    gnd_counts_filename = "/tmp/gnd_counts"
    if not util.ExecOrDie("/usr/local/bin/count_gnd_refs",
                          [ gnd_numbers_path, most_recent_titles_filename, gnd_counts_filename ]):
         util.SendEmail("Beacon Generator", "An unexpected error occurred: /usr/local/bin/count_gnd_refs failed!")

    # Generate a file with a timestamp in the Beacon format:
    timestamp_filename = "/tmp/beacon_timestamp"
    with open(timestamp_filename, "w") as timestamp_file:
        timestamp_file.write("#TIMESTAMP: " + str(datetime.date.today()) + "\n")

    # Now generate the final output (header + counts):
    if not util.ConcatenateFiles([ sys.argv[2], timestamp_filename, gnd_counts_filename ], sys.argv[3]):
         util.SendEmail("Beacon Generator", "An unexpected error occurred: could not write \""
                        + sys.argv[3] + "\"!", priority=1)

    # Cleanup of temp files:
    os.unlink(gnd_numbers_path)
    os.unlink(_035a_contents_filename)
    os.unlink(timestamp_filename)
    os.unlink(gnd_counts_filename)


try:
    Main()
except Exception as e:
    util.SendEmail("Beacon Generator", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
