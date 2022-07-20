#!/bin/python3
# -*- coding: utf-8 -*-
import datetime
import glob
import urllib.request, urllib.error, urllib.parse
import os
import sys
import time
import traceback
import util


# Clear the index to do away with old data that might remain otherwise
# Since no commit is executed here we avoid the empty index problem
def ClearSolrIndex(index):
    try:
        url = "http://localhost:8983/solr/" + index + "/update"
        values = "<delete><query>*:*</query></delete>"
        data = values.encode('utf-8')
        headers = {"Content-Type": "application/xml"}
        request = urllib.request.Request(url, data, headers)
        urllib.request.urlopen(request, timeout=300)
    except Exception as e:
        util.SendEmail("MARC-21 Pipeline", "Failed to clear the SOLR index \"" + index + "\" [" + str(e) + "]!", priority=1)
        sys.exit(-1)


def OptimizeSolrIndex(index):
    try:
        # since Solr 7.5, optimization is Solr-internally based on thresholds and therefore no longer forced to be executed on each call.
        # to force optimization every time, it would be necessary to add the maxSegments parameter to force optimization on each call.
        # see also: https://lucidworks.com/post/solr-optimize-merge-expungedeletes-tips/
        request = urllib.request.Request(
            "http://localhost:8983/solr/" + index + "/update?optimize=true")
        urllib.request.urlopen(request, timeout=3600)
    except:
        util.SendEmail("MARC-21 Pipeline", "Failed to optimize the SOLR index \"" + index + "\"!", priority=1)
        sys.exit(-1)


solrmarc_log_summary = "/tmp/solrmarc_log.summary"
import_log_summary = "/tmp/import_into_vufind_log.summary"


def ImportIntoVuFind(title_pattern, authority_pattern, log_file_name):
    vufind_dir = os.getenv("VUFIND_HOME");
    if vufind_dir == None:
        util.Error("VUFIND_HOME not set, cannot start solr import!")

    # import title data
    title_index = 'biblio'
    title_args = [ sorted(glob.glob(title_pattern), reverse=True)[0] ]
    if len(title_args) != 1:
        util.Error("\"" + title_pattern + "\" matched " + str(len(title_args))
                   + " files! (Should have matched exactly 1 file!)")
    ClearSolrIndex(title_index)
    util.ExecOrDie(vufind_dir + "/import-marc.sh", title_args, log_file_name)
    OptimizeSolrIndex(title_index)

    # import authority data
    authority_index = 'authority'
    authority_args = [ sorted(glob.glob(authority_pattern), reverse=True)[0] ]
    if len(authority_args) != 1:
        util.Error("\"" + authority_pattern + "\" matched " + str(len(authority_args))
                   + " files! (Should have matched exactly 1 file!)")
    ClearSolrIndex(authority_index)
    util.ExecOrDie(vufind_dir + "/import-marc-auth.sh", authority_args, log_file_name)
    OptimizeSolrIndex(authority_index)
    util.ExecOrDie(util.Which("sudo"), ["-u", "solr", "-E", vufind_dir + "/index-alphabetic-browse.sh"], log_file_name)

    # cleanup logs
    util.ExecOrDie("/usr/local/bin/summarize_logs", [vufind_dir + "/import/solrmarc.log", solrmarc_log_summary])
    util.ExecOrDie("/usr/local/bin/log_rotate", [vufind_dir + "/import/", "solrmarc\\.log"])
    util.ExecOrDie("/usr/local/bin/summarize_logs", [log_file_name, import_log_summary])
    util.ExecOrDie("/usr/local/bin/log_rotate", [os.path.dirname(log_file_name), os.path.basename(log_file_name)])


def RunPipelineAndImportIntoSolr(pipeline_script_name, marc_title, conf):
    log_file_name = util.MakeLogFileName(pipeline_script_name, util.GetLogDirectory())
    util.ExecOrDie(pipeline_script_name, [ marc_title ], log_file_name)
    log_file_name = util.MakeLogFileName("import_into_vufind", util.GetLogDirectory())
    ImportIntoVuFind(conf.get("FileNames", "title_marc_data"), conf.get("FileNames", "authority_marc_data"), log_file_name)

    # Write timestamp file for last successful Solr import:
    with open(os.open('/usr/local/vufind/public/last_solr_import', os.O_CREAT | os.O_WRONLY, 0o644), 'w') as output:
        output.write(str(datetime.datetime.now()))


# Returns True if we have no timestamp file or if link_filename's creation time is more recent than
# the time found in the timestamp file.
def FoundNewBSZDataFile(link_filename):
    try:
        statinfo = os.stat(link_filename)
        file_creation_time = statinfo.st_ctime
    except OSError:
        util.Error("in FoundNewBSZDataFile: Symlink \"" + link_filename + "\" is missing or dangling!")
    old_timestamp = util.ReadTimestamp()
    return old_timestamp < file_creation_time


REFTERM_MUTEX_FILE = "/usr/local/var/tmp/create_refterm_successful" # Must match mutex file name in create_refterm_file.py


def FoundReftermMutex():
    return os.path.exists(REFTERM_MUTEX_FILE)


def DeleteReftermMutex():
    os.remove(REFTERM_MUTEX_FILE)


def WriteImportFinishedFile():
    import_finished_file = '/usr/local/vufind/public/last_import.txt'
    with open(import_finished_file, "w") as import_finished:
        import_finished.write(time.strftime("%Y%m%d-%H%M%S"))


def Main():
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
         util.SendEmail("MARC-21 Pipeline Kick-Off (Failure)", "Pipeline script not found or not executable: \""
                        + pipeline_script_name + "\"\n", priority=1)
         sys.exit(-1)
    conf = util.LoadConfigFile()
    link_name = conf.get("Misc", "link_name")
    if FoundNewBSZDataFile(link_name):
        if not FoundReftermMutex():
             util.Error("No Refterm Mutex found")
        bsz_data = util.ResolveSymlink(link_name)
        if not bsz_data.endswith(".tar.gz"):
            util.Error("BSZ data file must end in .tar.gz!")
        file_name_list = util.ExtractAndRenameBSZFiles(bsz_data)

        RunPipelineAndImportIntoSolr(pipeline_script_name, file_name_list[0], conf)
        util.WriteTimestamp()
        DeleteReftermMutex()
        WriteImportFinishedFile()
        util.SendEmail("MARC-21 Pipeline", "Pipeline completed successfully.", priority=5,
                       attachments=[solrmarc_log_summary, import_log_summary])

    else:
        util.SendEmail("MARC-21 Pipeline Kick-Off", "No new data was found.", priority=5)


try:
    Main()
except Exception as e:
    error_msg =  "An unexpected error occurred: " + str(e) + "\n\n" + traceback.format_exc(20)
    util.SendEmail("MARC-21 Pipeline Kick-Off", error_msg, priority=1)
    sys.stderr.write(error_msg)
