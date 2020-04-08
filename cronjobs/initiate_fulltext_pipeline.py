#!/bin/python3
# -*- coding: utf-8 -*-
import datetime
import glob
import urllib.request, urllib.error, urllib.parse
import os
import pipeline_util
import struct
import sys
import time
import traceback
import util


def RunPipeline(pipeline_script_name, marc_title, conf):
    log_file_name = util.MakeLogFileName(pipeline_script_name, util.GetLogDirectory())
    util.ExecOrDie(pipeline_script_name, [ marc_title ], log_file_name)


def Main():
    util.default_email_sender = "initiate_fulltext_pipeline@ub.uni-tuebingen.de"
    if len(sys.argv) != 3:
         print("invalid arguments! usage: initiate_marc_pipeline.py <default email recipient> <MARC21 pipeline script name>")
         util.SendEmail("MARC-21 Pipeline Kick-Off (Failure)",
                        "This script needs to be called with two arguments,\n"
                        + "the default email recipient and the name of the MARC-21\n"
                        + "pipeline script to be executed.\n", priority=1)
         sys.exit(-1)

    util.default_email_recipient = sys.argv[1]
    pipeline_script_name = sys.argv[2]
    if not os.access(pipeline_script_name, os.X_OK):
        print("Pipeline script not found or not executable: " + pipeline_script_name)
        util.SendEmail("Fulltext Pipeline Kick-Off (Failure)", "Pipeline script not found or not executable: \""
                        + pipeline_script_name + "\"\n", priority=1)
        sys.exit(-1)
    conf = util.LoadConfigFile(util.default_config_file_dir + '/' + 'initiate_marc_pipeline.conf')
    link_name = conf.get("Misc", "link_name")
    if pipeline_util.FoundNewBSZDataFile(link_name):
        bsz_data = util.ResolveSymlink(link_name)
        if not bsz_data.endswith(".tar.gz"):
            util.Error("BSZ data file must end in .tar.gz!")
        file_name_list = util.ExtractAndRenameBSZFiles(bsz_data)
        RunPipeline(pipeline_script_name, file_name_list[0], conf)
        util.SendEmail("MARC-21 Pipeline", "Pipeline completed successfully.", priority=5)
    else:
        util.SendEmail("Fulltext Pipeline Kick-Off", "No new data was found.", priority=5)


try:
    Main()
except Exception as e:
    error_msg =  "An unexpected error occurred: " + str(e) + "\n\n" + traceback.format_exc(20)
    util.SendEmail("MARC-21 Pipeline Kick-Off", error_msg, priority=1)
    sys.stderr.write(error_msg)


