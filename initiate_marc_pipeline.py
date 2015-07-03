#!/usr/bin/python3


import exec
import os
import struct
import sys
import util


def StartPipeline(pipeline_script_name):
    if exec.Exec(pipeline_script_name, timeout=60*60) == 0:
        util.SendEmail("MARC-21 Pipeline", "Pipeline completed successfully.")
    else:
        util.SendEmail("MARC-21 Pipeline", "Pipeline failed.")


def Main():
    default_email_sender = "initiate_marc_pipeline@ub.uni-tuebingen.de"
    if len(sys.argv) != 2:
         util.SendEmail("MARC-21 Pipeline Kick-Off (Failure)",
                        "This script needs to be called with a single argument,\n"
                        + "the name of the MARC-21 pipeline script to be executed.\n")
         sys.exit(-1)
    pipeline_script_name = sys.argv[1]
    if not os.access(pipeline_script_name, os.X_OK):
         util.SendEmail("MARC-21 Pipeline Kick-Off (Failure)", "Pipeline script not found or not executable!\n")
         sys.exit(-1)
    util.default_email_sender = "initiate_marc_pipeline@ub.uni-tuebingen.de"
    conf = util.LoadConfigFile(sys.argv[0][:-2] + "conf")
    link_name = conf["Misc"]["link_name"]
    if util.FoundNewBSZDataFile(link_name):
        StartPipeline(pipeline_script_name)
    else:
        util.SendEmail("MARC-21 Pipeline Kick-Off", "No new data was found.")


try:
    Main()
except Exception as e:
    util.SendEmail("MARC-21 Pipeline Kick-Off", "An unexpected error occurred: " + str(e))
