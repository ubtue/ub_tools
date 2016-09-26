#!/bin/python2
# -*- coding: utf-8 -*-

import datetime
import glob
import os
import process_util
import struct
import sys
import tarfile
import traceback
import re
import urllib2
import util


def ExecOrDie(cmd_name, args, log_file_name):
    if not process_util.Exec(cmd_path=cmd_name, args=args, new_stdout=log_file_name,
                             new_stderr=log_file_name, append_stdout=True, append_stderr=True) == 0:
        util.SendEmail("Create Refterm File",  "Failed to execute \"" + cmd_name + "\".\nSee logfile \"" + log_file_name + "\" for the reason.", priority=1)
        sys.exit(-1)


def FoundNewBSZDataFile(link_filename):
    try:
        statinfo = os.stat(link_filename)
        file_creation_time = statinfo.st_ctime
    except OSError as e:
        util.Error("in FoundNewBSZDataFile: Symlink \"" + link_filename + "\" is missing or dangling!")
    old_timestamp = util.ReadTimestamp()
    return old_timestamp < file_creation_time


def ExtractRefDataMarcFile(gzipped_tar_archive, output_marc_file, log_file_name):
    ExecOrDie("/usr/local/bin/extract_refterm_archive.sh", [gzipped_tar_archive, output_marc_file], log_file_name)


def ExtractTitleDataMarcFile(link_name):
    bsz_data = util.ResolveSymlink(link_name)

    if not bsz_data.endswith(".tar.gz"):
        util.Error("BSZ data file must end in .tar.gz!")
    file_name_list = util.ExtractAndRenameBSZFiles(bsz_data)
    title_data_file_name = [ file_name for file_name in file_name_list if file_name.startswith('GesamtTiteldaten') ]
    return title_data_file_name[0]


def CreateRefTermFile(ref_data_archive, title_data_link_name, conf):
    log_file_name = util.MakeLogFileName(os.path.basename(__file__), util.GetLogDirectory())
    title_data_file_orig = ExtractTitleDataMarcFile(title_data_link_name)
    try: 
        date_string = re.search('\d{6}', title_data_file_orig).group()
    except AttributeError:
        date_string = ''
    # Make sure we will not interfere with filenames used by the ordinary pipeline
    title_data_file = "GesamtTiteldaten-" + date_string + "-refterm.mrc"
    os.rename(title_data_file_orig, title_data_file); 
    # Assemble Filenames 
    ref_data_base_filename = "Hinweissätze-" + date_string
    ref_data_marc_file = ref_data_base_filename + ".mrc"
    # Convert tar.gz to mrc
    ExtractRefDataMarcFile(ref_data_archive, ref_data_marc_file, log_file_name)
    ref_data_synonym_file = ref_data_base_filename + ".txt"
    # Make a refterm -> circumscription table file
    ExecOrDie("/usr/local/bin/extract_referenceterms", [ref_data_marc_file, ref_data_synonym_file] , log_file_name)
    # Setup a temporary solr instance in a ramdisk and import title data
    ExecOrDie("/usr/local/bin/setup_refterm_solr.sh", [title_data_file], log_file_name)
    # Create a file with a list of refterms and containing ids
    ExecOrDie("/usr/local/bin/create_reference_import_file.sh", [ref_data_synonym_file, os.getcwd()], log_file_name)
    # Terminate the temporary solr instance
    ExecOrDie("/usr/local/bin/shutdown_refterm_solr.sh", [] , log_file_name)
    # Clean up temporary title data
    util.Remove(title_data_file)
  

def Main():
    util.default_email_sender = "create_refterm_file@ub.uni-tuebingen.de"
    util.default_email_recipient = "johannes.riedl@uni-tuebingen.de"
    if len(sys.argv) != 2:
         util.SendEmail("Create Refterm File (Kickoff Failure)",
                        "This script must be called with one argument,\n"
                        + "the default email recipient\n", priority=1);
         sys.exit(-1)
    util.default_email_recipient = sys.argv[1]
    conf = util.LoadConfigFile()
    link_name = conf.get("Misc", "link_name")
    ref_data_pattern = conf.get("Hinweisabzug", "filename_pattern")
    ref_data_archive = util.getMostRecentFileMatchingGlob(ref_data_pattern)
    if ref_data_archive is None:
         util.SendEmail("Create Refterm File (No Reference Data File Found)",
                        "No File matching pattern \"" + ref_data_pattern + "\" found\n", priority=1)
   
    if FoundNewBSZDataFile(link_name):
        CreateRefTermFile(ref_data_archive, link_name, conf)
        util.SendEmail("Create Refterm File", "Refterm file successfully created.", priority=5)
    else:
        util.SendEmail("Create Refterm File", "No new data was found.", priority=5)


try:
    Main()
except Exception as e:
    error_msg =  "An unexpected error occurred: " + str(e) + "\n\n" + traceback.format_exc(20)
    util.SendEmail("Create Refterm File", error_msg, priority=1)
    sys.stderr.write(error_msg)



                         
         

