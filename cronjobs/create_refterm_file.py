#!/bin/python3
# -*- coding: utf-8 -*-
import atexit
import datetime
import multiprocessing
import os
import sys
import traceback
import process_util
import re
import util

# Must match path in initiate_marc_pipeline.py and, if applicable, trigger_pipeline_script.sh
REFTERM_MUTEX_FILE = "/usr/local/var/tmp/create_refterm_successful"


def ExecOrCleanShutdownAndDie(cmd_name, args, log_file_name=None):
    if log_file_name is None:
        log_file_name = "/proc/self/fd/2" # stderr
    if not process_util.Exec(cmd_path=cmd_name, args=args, new_stdout=log_file_name,
                             new_stderr=log_file_name, append_stdout=True, append_stderr=True, setsid=False) == 0:
        CleanUp(None, log_file_name)
        util.SendEmail("util.ExecOrDie", "Failed to execute \"" + cmd_name + "\".\nSee logfile \"" + log_file_name
                  + "\" for the reason.", priority=1)
        sys.exit(-1)


def FoundNewBSZDataFile(link_filename):
    try:
        statinfo = os.stat(link_filename)
        file_creation_time = statinfo.st_ctime
    except OSError:
        util.Error("in FoundNewBSZDataFile: Symlink \"" + link_filename + "\" is missing or dangling!")
    old_timestamp = util.ReadTimestamp("initiate_marc_pipeline")
    return old_timestamp < file_creation_time


def ExtractRefDataMarcFile(gzipped_tar_archive, output_marc_file, log_file_name):
    util.ExecOrDie("/usr/local/bin/extract_refterm_archive.sh", [gzipped_tar_archive, output_marc_file],
                   log_file_name)


def ExtractTitleDataMarcFile(link_name):
    bsz_data = util.ResolveSymlink(link_name)

    if not bsz_data.endswith(".tar.gz"):
        util.Error("BSZ data file must end in .tar.gz!")
    file_name_list = util.ExtractAndRenameBSZFiles(bsz_data)
    title_data_file_name = [ file_name for file_name in file_name_list if file_name.startswith('GesamtTiteldaten') ]
    return title_data_file_name[0]


def GetDateFromFilename(filename):
    try:
        date_string = re.search('\\d{6}', filename).group()
    except AttributeError:
        date_string = ''
    return date_string


def RenameTitleDataFile(title_data_file_orig, date_string):
    # Make sure we will not interfere with filenames used by the ordinary pipeline
    title_data_file = "GesamtTiteldaten-" + date_string + "-temporary.mrc"
    os.rename(title_data_file_orig, title_data_file);
    return title_data_file


def SetupTemporarySolrInstance(title_data_file, conf, log_file_name):
    # Setup a temporary solr instance in a ramdisk and import title data
    util.ExecOrDie("/usr/local/bin/setup_refterm_solr.sh", [title_data_file], log_file_name)


def CreateRefTermFile(ref_data_archive, date_string, conf, log_file_name):
    # Skip if unneeded
    if ref_data_archive is None:
        return

    # Assemble Filenames
    ref_data_base_filename = "Hinweissätze-" + date_string
    ref_data_marc_file = ref_data_base_filename + ".mrc"
    # Convert tar.gz to mrc
    ExtractRefDataMarcFile(ref_data_archive, ref_data_marc_file, log_file_name)
    ref_data_synonym_file = ref_data_base_filename + ".txt"
    # Make a refterm -> circumscription table file
    ExecOrCleanShutdownAndDie("/usr/local/bin/extract_referenceterms", [ref_data_marc_file, ref_data_synonym_file],
                   log_file_name)
    # Create a file with a list of refterms and containing ids
    ExecOrCleanShutdownAndDie("/usr/local/bin/create_reference_import_file.sh", [ref_data_synonym_file, os.getcwd()],
                   log_file_name)


def CreateSerialSortDate(title_data_file, date_string, log_file_name):
    serial_ppn_sort_list = "Schriftenreihen-Sortierung-" + date_string + ".txt"
    ExecOrCleanShutdownAndDie("/usr/local/bin/query_serial_sort_data.sh", [title_data_file, serial_ppn_sort_list], log_file_name)


# Extract existing Fulltext PPN's from the Elasticsearch instance
def CreateFulltextIdsFile(ids_output_file, log_file_name):
    elasticsearch_access_conf = "/usr/local/var/lib/tuelib/Elasticsearch.conf"
    if os.access(elasticsearch_access_conf, os.F_OK):
        util.ExecOrDie("/usr/local/bin/extract_existing_fulltext_ids.sh", [ ids_output_file ], log_file_name)
    else: # Skip if configuration is not present
        util.ExecOrDie(util.Which("truncate"), [ "-s", "0",  log_file_name ])
        util.ExecOrDie(util.Which("echo"), [ "Skip extraction since " + elasticsearch_access_conf + " not present" ], log_file_name)


# Create the database for matching fulltext to vufind entries
def CreateMatchDB(title_marc_data, log_file_name):
    util.ExecOrDie("/usr/local/bin/create_match_db", [ title_marc_data ], log_file_name, setsid=False);


def CreateLogFile():
    return util.MakeLogFileName(os.path.basename(__file__), util.GetLogDirectory())


def CleanUp(title_data_file, log_file_name):
    # Terminate the temporary solr instance
    util.ExecOrDie("/usr/local/bin/shutdown_refterm_solr.sh", [] , log_file_name)
    # Clean up temporary title data
    if title_data_file is not None:
        util.Remove(title_data_file)


def ExecuteInParallel(*processes):
    for process in processes:
        process.start()

    for process in processes:
        process.join()
        if process.exitcode != 0:
            util.Error(process.name + " failed")


def CleanStaleMutex():
    if os.path.exists(REFTERM_MUTEX_FILE):
       os.remove(REFTERM_MUTEX_FILE)


def Main():
    util.default_email_recipient = "johannes.riedl@uni-tuebingen.de"
    if len(sys.argv) != 2:
         util.SendEmail("Create Refterm File (Kickoff Failure)",
                        "This script must be called with one argument,\n"
                        + "the default email recipient\n", priority=1);
         sys.exit(-1)
    util.default_email_recipient = sys.argv[1]
    CleanStaleMutex()
    conf = util.LoadConfigFile()
    title_data_link_name = conf.get("Misc", "link_name")
    ref_data_pattern = conf.get("Hinweisabzug", "filename_pattern")
    if ref_data_pattern != "" :
        ref_data_archive = util.getMostRecentFileMatchingGlob(ref_data_pattern)
        if ref_data_archive is None:
            util.SendEmail("Create Refterm File (No Reference Data File Found)",
                           "No File matching pattern \"" + ref_data_pattern + "\" found\n", priority=1)
    else:
        ref_data_archive = None

    if FoundNewBSZDataFile(title_data_link_name):
        start = datetime.datetime.now()
        log_file_name = CreateLogFile()
        title_data_file_orig = ExtractTitleDataMarcFile(title_data_link_name)
        date_string = GetDateFromFilename(title_data_file_orig)
        title_data_file = RenameTitleDataFile(title_data_file_orig, date_string)
        atexit.register(CleanUp, title_data_file, log_file_name)
        setup_temporary_solr_instance_process = multiprocessing.Process(target=SetupTemporarySolrInstance,
                                                                        name="Setup temporary Solr instance",
                                                                        args=[ title_data_file, conf, log_file_name ])
        create_match_db_log_file_name = util.MakeLogFileName("create_match_db", util.GetLogDirectory())
        create_match_db_process = multiprocessing.Process(target=CreateMatchDB, name="Create Match DB",
                                      args=[ title_data_file, create_match_db_log_file_name ])
        ExecuteInParallel(setup_temporary_solr_instance_process, create_match_db_process)
        create_ref_term_process = multiprocessing.Process(target=CreateRefTermFile, name="Create Reference Terms File",
                                      args=[ ref_data_archive, date_string, conf, log_file_name ])
        create_serial_sort_term_process = multiprocessing.Process(target=CreateSerialSortDate, name="Serial Sort Date",
                                              args=[ title_data_file, date_string, log_file_name ])
        extract_fulltext_ids_log_file_name = util.MakeLogFileName("extract_fulltext_ids", util.GetLogDirectory())
        extract_fulltext_ids_process = multiprocessing.Process(target=CreateFulltextIdsFile, name="Create Fulltext IDs File",
                                           args=[ "/usr/local/ub_tools/bsz_daten/fulltext_ids.txt", extract_fulltext_ids_log_file_name ])
        ExecuteInParallel(create_ref_term_process, create_serial_sort_term_process, extract_fulltext_ids_process)
        end  = datetime.datetime.now()
        duration_in_minutes = str((end - start).seconds / 60.0)
        util.Touch(REFTERM_MUTEX_FILE)
        util.SendEmail("Create Refterm File", "Refterm file successfully created in " + duration_in_minutes + " minutes.", priority=5)
    else:
        util.SendEmail("Create Refterm File", "No new data was found.", priority=5)


try:
    Main()
except Exception as e:
    error_msg =  "An unexpected error occurred: " + str(e) + "\n\n" + traceback.format_exc(20)
    util.SendEmail("Create Refterm File", error_msg, priority=1)
    sys.stderr.write(error_msg)
