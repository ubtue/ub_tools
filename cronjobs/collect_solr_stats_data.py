#!/bin/python2
# -*- coding: utf-8 -*-
#
# A driver for the collect_solr_stats_data.cc program.

import os
import sys
import traceback
import util


def Main():
    if len(sys.argv) != 3:
        util.SendEmail(os.path.basename(sys.argv[0]),
                       "This script needs to be called with an email address and the system type!\n", priority=1)
        sys.exit(-1)
    util.default_email_recipient = sys.argv[1]

    system_type = sys.argv[2]
    if system_type != "krimdok" and system_type != "relbib" and system_type != "ixtheo":
        util.SendEmail(os.path.basename(sys.argv[0]),
                       "This system_type must be one of {krimdok,relbib,ixtheo}!\n", priority=1)
        sys.exit(-1)

    output_file = "/tmp/collect_solr_stats_data.csv"
    os.unlink(output_file)
    util.ExecOrDie("/usr/local/bin/collect_solr_stats_data", [ system_type, output_file ],
                   "/usr/local/var/log/tuelib/collect_solr_stats_data.log")

    # Mail CSV file to Ingo for futher processing:
    util.SendEmail(system_type + "Solr Stats", "CSV-Datei im Anhang.\n", recipient="johannes.ruscheinski@uni-tuebingen.de",
                   attachments=output_file)

    util.SendEmail("Solr Stats Collector", "Successfully generated and emailed Solr statistics.", priority=5)


try:
    Main()
except Exception as e:
    util.SendEmail("Solr Stats Collector", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
