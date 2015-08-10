#!/bin/python2
# -*- coding: utf-8 -*-


from __future__ import print_function
import datetime
import process_util
import os
import struct
import sys
import traceback
import util


def ExecOrDie(cmd_name, args, log_file_name):
    if not process_util.Exec(cmd_path=cmd_name, args=args, timeout=60*60,
                             new_stdout=log_file_name, new_stderr=log_file_name) == 0:
        util.SendEmail("MARC-21 Pipeline",  "Pipeline failed.  See logs in /tmp for the reason.")
        sys.exit(-1)

    
def StartPipeline(pipeline_script_name, data_files):
    log_file_name = util.MakeLogFileName(pipeline_script_name)
    ExecOrDie(pipeline_script_name, data_files, log_file_name)

    deletion_list_glob = "LOEPPN-[0-9][0-9][0-9][0-9][0-9][0-9]"
    most_recent_deletion_list = util.getMostRecentFileMatchingGlob(deletion_list_glob)
    if not most_recent_deletion_list:
        util.SendEmail("MARC-21 Pipeline",  "Did not find any files matching \"" + deletion_list_glob + "\".")
    delete_solr_ids_args = [ util.default_email_recipient, most_recent_deletion_list ]
    ExecOrDie("/usr/local/bin/delete_solr_ids.sh", delete_solr_ids_args, log_file_name)

    args = [
        "ÜbergeordneteTitelUndLokaldaten-filtered-and-normalised-with-child-refs-[0-9][0-9][0-9][0-9][0-9][0-9].mrc"]
    ExecOrDie("/usr/local/vufind2/import-marc.sh", args, log_file_name)

    args = [
        "TitelUndLokaldaten-normalised-with-issns-and-full-text-links-[0-9][0-9][0-9][0-9][0-9][0-9].mrc"]
    ExecOrDie("/usr/local/vufind2/import-marc.sh", args, log_file_name)


def Main():
    util.default_email_sender = "initiate_marc_pipeline@ub.uni-tuebingen.de"
    if len(sys.argv) != 3:
         util.SendEmail("MARC-21 Pipeline Kick-Off (Failure)",
                        "This script needs to be called with two arguments,\n"
                        + "the default email recipient and the name of the MARC-21\n"
                        + "pipeline script to be executed.\n")
         sys.exit(-1)
    util.default_email_recipient = sys.argv[1]
    pipeline_script_name = sys.argv[2]
    if not os.access(pipeline_script_name, os.X_OK):
         util.SendEmail("MARC-21 Pipeline Kick-Off (Failure)", "Pipeline script not found or not executable!\n")
         sys.exit(-1)
    conf = util.LoadConfigFile()
    link_name = conf.get("Misc", "link_name")
    if util.FoundNewBSZDataFile(link_name):
        bsz_data = util.ResolveSymlink(link_name)
        if not bsz_data.endswith(".tar.gz"):
            util.Error("BSZ data file must end in .tar.gz!")
        file_name_list = util.ExtractAndRenameBSZFiles(bsz_data)
        
        StartPipeline(pipeline_script_name, file_name_list)
        util.SendEmail("MARC-21 Pipeline", "Pipeline completed successfully.")
    else:
        util.SendEmail("MARC-21 Pipeline Kick-Off", "No new data was found.")


try:
    Main()
except Exception as e:
    util.SendEmail("MARC-21 Pipeline Kick-Off", "An unexpected error occurred: " + str(e)
                   + "\n\n" + traceback.format_exc(20))
