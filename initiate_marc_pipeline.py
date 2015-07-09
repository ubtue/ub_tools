#!/bin/python2
# -*- coding: utf-8 -*-


from __future__ import print_function
import datetime
import process_util
import os
import struct
import sys
import tarfile
import util


def StartPipeline(pipeline_script_name, data_files):
    if process_util.Exec(cmd_path=pipeline_script_name, args=data_files, timeout=60*60) == 0:
        util.SendEmail("MARC-21 Pipeline", "Pipeline completed successfully.")
    else:
        util.SendEmail("MARC-21 Pipeline", "Pipeline failed.")


def ExtractAndRenameMember(tar_file, member, new_name):
    util.Remove(member)
    tar_file.extract(member)
    util.Remove(new_name)
    os.rename(member, new_name)


def ExtractAndRenameBSZFiles(gzipped_tar_archive):
    tar_file = tarfile.open(gzipped_tar_archive, "r:gz")
    members = tar_file.getnames()
    current_date_str = datetime.datetime.now().strftime("%d%m%y")
    for member in members:
        if member.endswith("a001.raw"):
            ExtractAndRenameMember(tar_file, member, "TitelUndLokaldaten-" + current_date_str + ".mrc")
        elif member.endswith("b001.raw"):
            ExtractAndRenameMember(tar_file, member, "ÜbergeordneteTitelUndLokaldaten-" + current_date_str + ".mrc")
        elif member.endswith("c001.raw"):
            ExtractAndRenameMember(tar_file, member, "Normdaten-" + current_date_str + ".mrc")
        else:
            util.error("Unknown member \"" + member + "\" in archive \"" + gzipped_tar_archive + "\"!")
    return ["TitelUndLokaldaten-" + current_date_str + ".mrc",
            "ÜbergeordneteTitelUndLokaldaten-" + current_date_str + ".mrc",
            "Normdaten-" + current_date_str + ".mrc"]


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
    conf = util.LoadConfigFile("initiate_marc_pipeline.conf")
    link_name = conf.get("Misc", "link_name")
    if util.FoundNewBSZDataFile(link_name):
        bsz_data = util.ResolveSymlink(link_name)
        if not bsz_data.endswith(".tar.gz"):
            util.Error("BSZ data file must end in .tar.gz!")
        file_name_list = ExtractAndRenameBSZFiles(bsz_data)
        
        StartPipeline(pipeline_script_name, file_name_list)
    else:
        util.SendEmail("MARC-21 Pipeline Kick-Off", "No new data was found.")


try:
    Main()
except Exception as e:
    util.SendEmail("MARC-21 Pipeline Kick-Off", "An unexpected error occurred: " + str(e))
