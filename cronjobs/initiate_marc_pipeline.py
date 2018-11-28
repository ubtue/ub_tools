#!/bin/python2
# -*- coding: utf-8 -*-

from multiprocessing import Process

import datetime
import glob
import urllib2
import os
import struct
import sys
import traceback
import util


# Clear the index to do away with old data that might remain otherwise
# Since no commit is executed here we avoid the empty index problem
def ClearSolrIndex(index):
    try:
        request = urllib2.Request(
            "http://localhost:8080/solr/" + index + "/update?stream.body=%3Cdelete%3E%3Cquery%3E*:*%3C/query%3E%3C/delete%3E")
        response = urllib2.urlopen(request, timeout=60)
    except:
        util.SendEmail("MARC-21 Pipeline", "Failed to clear the SOLR index \"" + index + "\"!", priority=1)
        sys.exit(-1)


def OptimizeSolrIndex(index):
    try:
        request = urllib2.Request(
            "http://localhost:8080/solr/" + index + "/update?optimize=true")
        response = urllib2.urlopen(request, timeout=1800)
    except:
        util.SendEmail("MARC-21 Pipeline", "Failed to optimize the SOLR index \"" + index + "\"!", priority=1)
        sys.exit(-1)


solrmarc_log_summary = "/tmp/solrmarc_log.summary"
import_log_summary = "/tmp/import_into_vufind_log.summary"


def ImportIntoVuFind(title_file_name, authority_file_name, log_file_name):
    vufind_dir = os.getenv("VUFIND_HOME");
    if vufind_dir == None:
        util.Error("VUFIND_HOME not set, cannot start solr import!")

    # import title data
    title_index = 'biblio'
    ClearSolrIndex(title_index)
    util.ExecOrDie(vufind_dir + "/import-marc.sh", [ title_file_name ], log_file_name, setsid=False)
    OptimizeSolrIndex(title_index)
    util.ExecOrDie(util.Which("sudo"), [ "-u", "solr", "-E", vufind_dir + "/index-alphabetic-browse.sh" ],
                   log_file_name, setsid=False)

    # import authority data
    authority_index = 'authority'
    ClearSolrIndex(authority_index)
    util.ExecOrDie(vufind_dir + "/import-marc-auth.sh", [ authority_file_name ], log_file_name, setsid=False)
    OptimizeSolrIndex(authority_index)


# Create the database for matching fulltext to vufind entries
def CreateMatchDB(title_marc_data, log_file_name):
    util.ExecOrDie("/usr/local/bin/create_match_db", title_marc_data, log_file_name, setsid=False);


def CleanupLogs(pipeline_log_file_name, create_match_db_log_file_name):
    vufind_dir = os.getenv("VUFIND_HOME");
    if vufind_dir == None:
        util.Error("VUFIND_HOME not set, cannot handle log files")
    util.ExecOrDie("/usr/local/bin/summarize_logs", [vufind_dir + "/import/solrmarc.log", solrmarc_log_summary])
    util.ExecOrDie("/usr/local/bin/log_rotate", [vufind_dir + "/import/", "solrmarc\\.log"])
    util.ExecOrDie("/usr/local/bin/summarize_logs", [pipeline_log_file_name, import_log_summary])
    util.ExecOrDie("/usr/local/bin/log_rotate", [os.path.dirname(pipeline_log_file_name), os.path.basename(pipeline_log_file_name)])
    util.ExecOrDie("/usr/local/bin/log_rotate", [os.path.dirname(create_match_db_log_file_name),
                                                 os.path.basename(create_match_db_log_file_name)])


def GetTitleAfterPipelineFileName(conf):
    title_marc_pattern = conf.get("FileNames", "title_marc_data")
    return sorted(glob.glob(title_marc_pattern), reverse=True)[0]


def GetAuthorityFileName(conf):
    authority_marc_pattern = conf.get("FileNames", "authority_marc_data")
    return sorted(glob.glob(authority_marc_pattern), reverse=True)[0]


def ExecutePipeline(pipeline_script_name, marc_title_before_pipeline, conf):
    pipeline_log_file_name = util.MakeLogFileName(pipeline_script_name, util.GetLogDirectory())
    util.ExecOrDie(pipeline_script_name, [ marc_title_before_pipeline ], pipeline_log_file_name)
    marc_title_after_pipeline = GetTitleAfterPipelineFileName(conf)
    authority_file_name = GetAuthorityFileName(conf)
    import_vufind_log_file_name = util.MakeLogFileName("import_into_vufind", util.GetLogDirectory())
    import_into_vufind_process = Process(target=ImportIntoVuFind,
                                         args=[ marc_title_after_pipeline, authority_file_name, import_vufind_log_file_name ])
    import_into_vufind_process.start()
    create_match_db_log_file_name = util.MakeLogFileName("create_match_db", util.GetLogDirectory())
    create_match_db_process = Process(target=CreateMatchDB,args=[ marc_title_after_pipeline, create_match_db_log_file_name ]
    create_match_db_process.start()
    import_into_vufind_process.join()
    if import_into_vufind_process.exitcode != 0:
        util.Error("The vufind import process failed")
    create_match_db_process.join()
    if import_into_vufind_process.exitcode != 0:
        util.Error("The create match db process failed");
    CleanupLogs(import_vufind_log_file_name, create_match_db_log_file_name)


# Returns True if we have no timestamp file or if link_filename's creation time is more recent than
# the time found in the timestamp file.
def FoundNewBSZDataFile(link_filename):
    try:
        statinfo = os.stat(link_filename)
        file_creation_time = statinfo.st_ctime
    except OSError as e:
        util.Error("in FoundNewBSZDataFile: Symlink \"" + link_filename + "\" is missing or dangling!")
    old_timestamp = util.ReadTimestamp()
    return old_timestamp < file_creation_time


def Main():
    util.default_email_sender = "initiate_marc_pipeline@ub.uni-tuebingen.de"
    if len(sys.argv) != 3:
         print "invalid arguments! usage: initiate_marc_pipeline.py <default email recipient> <MARC21 pipeline script name>"
         util.SendEmail("MARC-21 Pipeline Kick-Off (Failure)",
                        "This script needs to be called with two arguments,\n"
                        + "the default email recipient and the name of the MARC-21\n"
                        + "pipeline script to be executed.\n", priority=1)
         sys.exit(-1)

    util.default_email_recipient = sys.argv[1]
    pipeline_script_name = sys.argv[2]
    if not os.access(pipeline_script_name, os.X_OK):
         print "Pipeline script not found or not executable: " + pipeline_script_name
         util.SendEmail("MARC-21 Pipeline Kick-Off (Failure)", "Pipeline script not found or not executable: \""
                        + pipeline_script_name + "\"\n", priority=1)
         sys.exit(-1)
    conf = util.LoadConfigFile()
    link_name = conf.get("Misc", "link_name")
    if FoundNewBSZDataFile(link_name):
        bsz_data = util.ResolveSymlink(link_name)
        if not bsz_data.endswith(".tar.gz"):
            util.Error("BSZ data file must end in .tar.gz!")
        file_name_list = util.ExtractAndRenameBSZFiles(bsz_data)
        ExecutePipeline(pipeline_script_name, file_name_list[0], conf)
        util.SendEmail("MARC-21 Pipeline", "Pipeline completed successfully.", priority=5,
                       attachments=[solrmarc_log_summary, import_log_summary])
        util.WriteTimestamp()
    else:
        util.SendEmail("MARC-21 Pipeline Kick-Off", "No new data was found.", priority=5)


try:
    Main()
except Exception as e:
    error_msg =  "An unexpected error occurred: " + str(e) + "\n\n" + traceback.format_exc(20)
    util.SendEmail("MARC-21 Pipeline Kick-Off", error_msg, priority=1)
    sys.stderr.write(error_msg)
