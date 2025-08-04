#!/bin/python3
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
import codecs


def GetMostRecentBSZFile(filename_pattern: str):
    try:
        filename_regex = re.compile(filename_pattern)
    except Exception as e:
        util.Error("filename pattern \"" + filename_pattern + "\" failed to compile! (" + str(e) + ")")
    most_recent_date = "000000"
    most_recent_file = None
    for filename in os.listdir("/usr/local/ub_tools/bsz_daten/"):
        match = filename_regex.match(filename)
        if match and match.group(1) > most_recent_date:
            most_recent_date = match.group(1)
            most_recent_file = filename
    return None if most_recent_file is None else "/usr/local/ub_tools/bsz_daten/" + most_recent_file


def SendUsageAndExit():
    util.SendEmailAndExit(os.path.basename(sys.argv[0]),
                          "This script needs to be called with an optional --filter-field=tag, followed by an email address, "
                          "the beacon header file, an output path and an optional ppn-filter file as arguments!\n", priority=1)


def Main():
    if len(sys.argv) != 4 and len(sys.argv) != 5 and len(sys.argv) != 6 \
            or (len(sys.argv) == 6 and not sys.argv[1].startswith("--filter-field=")):
        SendUsageAndExit()

    count_author_gnd_refs_args = [];
    if sys.argv[1].startswith("--filter-field="):
        count_author_gnd_refs_args.append(sys.argv[1])
        del sys.argv[1]

    if len(sys.argv) != 4 and len(sys.argv) != 5:
        SendUsageAndExit()

    util.default_email_recipient = sys.argv[1]

    most_recent_authority_filename = GetMostRecentBSZFile("^Normdaten-(\\d\\d\\d\\d\\d\\d).mrc$")
    if most_recent_authority_filename is None:
        util.SendEmailAndExit("Beacon Generator", "Found no matching authority files!", priority=1)

    most_recent_titles_filename = GetMostRecentBSZFile("^GesamtTiteldaten-post-pipeline-(\\d\\d\\d\\d\\d\\d).mrc$")
    if most_recent_titles_filename is None:
        util.SendEmailAndExit("Beacon Generator", "Found no matching title files!", priority=1)

    # Extract the GND numbers from the 035$a subfield of the MARC authority data for authors:
    gnd_numbers_path = "/tmp/gnd_numbers"
    util.ExecOrDie("/usr/local/bin/extract_person_gnd_numbers", [ most_recent_authority_filename ], gnd_numbers_path)

    # Count GND references in the title data:
    gnd_counts_filename = "/tmp/gnd_counts"
    if len(sys.argv) > 4:
        count_author_gnd_refs_args.append("--control-number-list=" + sys.argv[4])
    count_author_gnd_refs_args.extend([ gnd_numbers_path, most_recent_titles_filename, gnd_counts_filename ])
    util.ExecOrDie("/usr/local/bin/count_author_gnd_refs", count_author_gnd_refs_args)

    # Generate a file with a timestamp in the Beacon format:
    timestamp_filename = "/tmp/beacon_timestamp"
    with open(timestamp_filename, "w") as timestamp_file:
        timestamp_file.write("#TIMESTAMP: " + str(datetime.date.today()) + "\n")

    # Create if not exist a bom file with utf-8
    file_with_utf8 = "/tmp/file_with_utf8"
    f = codecs.open(file_with_utf8, 'w', 'utf-8')
    f.write(u'\ufeff')
    f.close()

    # Now generate the final output (bom-utf8 + header + counts):
    if not util.ConcatenateFiles([file_with_utf8, sys.argv[2], timestamp_filename, gnd_counts_filename], sys.argv[3]):
        util.SendEmailAndExit("Beacon Generator", "An unexpected error occurred: could not write \"" + sys.argv[3] + "\"!", priority=1)

    # Cleanup of temp files:
    os.unlink(gnd_numbers_path)
    os.unlink(timestamp_filename)
    os.unlink(gnd_counts_filename)
    os.unlink(file_with_utf8)

    util.SendEmailAndExit("Beacon File Generator", "Successfully created a Beacon file.", priority=5)


try:
    Main()
except Exception as e:
    util.SendEmail("Beacon Generator", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
