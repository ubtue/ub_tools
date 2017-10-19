#!/bin/python2
# -*- coding: utf-8 -*-


import datetime
import glob
import urllib2
import os
import struct
import sys
import traceback
import util


# Delete the index to do away with old data that might remain otherwise
# Since no commit is executed here we avoid the empty index problem
def DeleteSolrIndex():
    try:
        request = urllib2.Request(
            "http://localhost:8080/solr/biblio/update?stream.body=%3Cdelete%3E%3Cquery%3E*:*%3C/query%3E%3C/delete%3E")
        response = urllib2.urlopen(request, timeout=60)
    except:
        util.SendEmail("MARC-21 Pipeline", "Failed to clear the SOLR index!", priority=1)
        sys.exit(-1)


def OptimizeSolrIndex():
    try:
        request = urllib2.Request(
            "http://localhost:8080/solr/biblio/update?optimize=true")
        response = urllib2.urlopen(request, timeout=1200)
    except:
        util.SendEmail("MARC-21 Pipeline", "Failed to optimize the SOLR index!", priority=1)
        sys.exit(-1)


solrmarc_log_summary = "/tmp/solrmarc_log.summary"


def ImportIntoVuFind(pattern, log_file_name):
    args = [ sorted(glob.glob(pattern), reverse=True)[0] ]
    if len(args) != 1:
        util.Error("\"" + pattern + "\" matched " + str(len(args))
                   + " files! (Should have matched exactly 1 file!)")
    DeleteSolrIndex()
    util.ExecOrDie("/usr/local/vufind/import-marc.sh", args, log_file_name)
    util.ExecOrDie("/usr/local/bin/summarize_logs", ["/usr/local/vufind/import/solrmarc.log", solrmarc_log_summary],
                   log_file_name)
    util.ExecOrDie("/usr/local/bin/log_rotate ", ["/usr/local/vufind/import/", "solrmarc\\.log"], log_file_name)
    OptimizeSolrIndex()
    util.ExecOrDie("sudo -u solr -E /usr/local/vufind/index-alphabetic-browse.sh", None, log_file_name)


def StartPipeline(pipeline_script_name, marc_title, conf):
    log_file_name = util.MakeLogFileName(pipeline_script_name, util.GetLogDirectory())
    util.ExecOrDie(pipeline_script_name, [ marc_title ], log_file_name)

    deletion_list_glob = "LOEPPN-[0-9][0-9][0-9][0-9][0-9][0-9]"
    most_recent_deletion_list = util.getMostRecentFileMatchingGlob(deletion_list_glob)
    if not most_recent_deletion_list:
        util.SendEmail("MARC-21 Pipeline", "Did not find any files matching \"" + deletion_list_glob + "\".",
                       priority=5)
    else:
        delete_solr_ids_args = [ util.default_email_recipient, most_recent_deletion_list ]
        util.ExecOrDie("/usr/local/bin/delete_solr_ids.sh", delete_solr_ids_args, log_file_name)

    log_file_name = util.MakeLogFileName("import_into_vufind", util.GetLogDirectory())
    ImportIntoVuFind(conf.get("FileNames", "title_marc_data"), log_file_name)


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
         util.SendEmail("MARC-21 Pipeline Kick-Off (Failure)",
                        "This script needs to be called with two arguments,\n"
                        + "the default email recipient and the name of the MARC-21\n"
                        + "pipeline script to be executed.\n", priority=1)
         sys.exit(-1)

    util.default_email_recipient = sys.argv[1]
    pipeline_script_name = sys.argv[2]
    if not os.access(pipeline_script_name, os.X_OK):
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

        StartPipeline(pipeline_script_name, file_name_list[0], conf)
        util.SendEmail("MARC-21 Pipeline", "Pipeline completed successfully.", priority=5,
                       attachment=solrmarc_log_summary)
        util.WriteTimestamp()
    else:
        util.SendEmail("MARC-21 Pipeline Kick-Off", "No new data was found.", priority=5)


try:
    Main()
except Exception as e:
    error_msg =  "An unexpected error occurred: " + str(e) + "\n\n" + traceback.format_exc(20)
    util.SendEmail("MARC-21 Pipeline Kick-Off", error_msg, priority=1)
    sys.stderr.write(error_msg)
