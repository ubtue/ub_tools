#!/bin/python2
# -*- coding: utf-8 -*-
#
# A tool that creates a pseudo complete MARC data download from the BSZ from a differential download
# and deletion lists and then starts a MARC-21 pipeline.
# A typical config file looks like this:
"""
[Files]
loesch_liste    = LOEPPN-current
komplett_abzug  = WA-MARC-krimdok-current.tar.gz
differenz_abzug = SA-MARC-krimdok-current.tar.gz

[SMTPServer]
server_address  = smtpserv.uni-tuebingen.de
server_user     = XXXXXX
server_password = XXXXXX
"""


from __future__ import print_function
from ftplib import FTP
import datetime
import glob
import os
import process_util
import re
import shutil
import sys
import traceback
import util


# Create a new deletion list named "augmented_list" which consists of a copy of "orig_list" and the
# list of extracted control numbers from "changed_marc_data".
def AugmentDeletionList(orig_list, changed_marc_data, augmented_list):
    util.Remove(augmented_list)
    shutil.copyfile(orig_list, augmented_list)
    if process_util.Exec("extract_IDs_in_erase_format.sh", args=[changed_marc_data, augmented_list],
                         timeout=100) != 0:
        util.Error("Failed to create \"" + augmented_list + "\"!")
    print("Successfully created \"" + augmented_list + "\".")


def DeleteMarcRecords(original_marc_file, deletion_list, processed_marc_file):
    util.Remove(processed_marc_file)
    if process_util.Exec("delete_ids", args=[deletion_list, original_marc_file, processed_marc_file],
                         timeout=200) != 0:
        util.Error("Failed to create \"" + processed_marc_file + "\"!")
    print("Successfully created \"" + processed_marc_file + "\".")


# Creates our empty data directory and changes into it.
def PrepareDataDirectory():
    data_dir = os.path.basename(sys.argv[0][:-3]) + ".data_dir"
    shutil.rmtree(data_dir, ignore_errors=True)
    os.mkdir(data_dir)
    os.chdir(data_dir)
    return data_dir


def RemoveOrDie(path):
    if not util.Remove(path):
        util.Error("Failed to delete \"" + path + "\"!")


def GetPathOrDie(executable):
    full_path = util.Which(executable)
    if full_path is None:
        util.Error("Can't find \"" + executable + "\" in our PATH!")
    return full_path


def RenameOrDie(old_name, new_name):
    try:
        os.rename(old_name, new_name)
    except:
        util.Error("Rename from \"" + old_name + "\" to \"" + new_name + "\" failed!")


# Iterates over norm, title and superior MARC-21 data sets and applies
# differential updates as well as deletions.  Should this function finish
# successfully, the three files "Normdaten-DDMMYY.mrc", "TitelUndLokaldaten-DDMMYY.mrc",
# and "ÜbergeordneteTitelUndLokaldaten-DDMMYY.mrc" will be left in our data
# directory.  Here DDMMYY is the current date.  Also changes into the parent directory.
def UpdateAllMarcFiles(orig_deletion_list):
    # Create a deletion list that consists of the original list from the
    # BSZ as well as all the ID's from the files starting w/ "Diff":
    util.Remove("augmented_deletion_list")
    shutil.copyfile(orig_deletion_list, "augmented_deletion_list")
    extract_IDs_script_path = GetPathOrDie("extract_IDs_in_erase_format.sh")
    for marc_file_name in glob.glob("*.mrc"):
        if not marc_file_name.startswith("Diff"):
            continue
        if process_util.Exec(extract_IDs_script_path,
                             args=[marc_file_name, "augmented_deletion_list"],
                             timeout=100) != 0:
            util.Error("Failed to append ID's from \"" + marc_file_name
                       + "\" to \"augmented_deletion_list\"!")
    print("Created an augmented deletion list.")

    # Now delete ID's from the augmented deletion list from all MARC-21 files:
    delete_ids_path = GetPathOrDie("delete_ids")
    for marc_file_name in glob.glob("*.mrc"):
        if marc_file_name.startswith("Diff"):
            continue
        trimmed_marc_file = marc_file_name[:-4] + "-trimmed.mrc"
        if process_util.Exec(delete_ids_path, args=["augmented_deletion_list", marc_file_name, trimmed_marc_file],
                             timeout=200, new_stdout="/dev/null", new_stderr="/dev/null") != 0:
            util.Error("Failed to create \"" + trimmed_marc_file + "\"!")
        RemoveOrDie(marc_file_name)
    RemoveOrDie("augmented_deletion_list")
    print("Deleted ID's from MARC files.")

    # Now concatenate the changed MARC records with the trimmed data sets:
    for marc_file_name in glob.glob("*-trimmed.mrc"):
        root_name = marc_file_name[:-19]
        diff_name = glob.glob("Diff" + root_name + "*.mrc")[0]
        if not util.ConcatenateFiles([marc_file_name, diff_name], root_name + ".mrc"):
            util.Error("We failed to concatenate \"" + marc_file_name + "\" and \"" + diff_name + "\"!")
        RemoveOrDie(marc_file_name)
        RemoveOrDie(diff_name)
    print("Created concatenated MARC files.")

    # Rename files to include the current date and move them up a directory:
    current_date_str = datetime.datetime.now().strftime("%d%m%y")
    for marc_file_name in glob.glob("*.mrc"):
        RenameOrDie(marc_file_name, "../" + marc_file_name[:-4] + "-" + current_date_str + ".mrc")
    os.chdir("..")
    print("Reamed and moved files.")

    # Create symlinks with "current" instead of "MMDDYY" in the orginal files:
    for marc_file_name in glob.glob("*-[0-9][0-9][0-9][0-9][0-9][0-9].mrc"):
        util.SafeSymlink(marc_file_name, re.sub("\\d\\d\\d\\d\\d\\d", "current", marc_file_name))
    print("Symlinked files.")


def Main():
    util.default_email_sender = "handle_partial_updates@ub.uni-tuebingen.de"
    if len(sys.argv) != 2:
        util.Error("This script expects one argument: default_email_recipient")
    util.default_email_recipient = sys.argv[1]
    config = util.LoadConfigFile()
    try:
        deletion_list     = config.get("Files", "loesch_liste")
        complete_data     = config.get("Files", "komplett_abzug")
        differential_data = config.get("Files", "differenz_abzug")
    except Exception as e:
        util.Error("failed to read config file! ("+ str(e) + ")")
    if (not os.access(deletion_list, os.R_OK) or not os.access(complete_data, os.R_OK)
        or not os.access(differential_data, os.R_OK)):
        util.SendEmail("Fehlende Daten vom BSZ?",
                       "Fehlende Löschliste, Komplettabzug oder Differenzabzug oder fehlende Zugriffsrechte.\n")

    # Bail out if the most recent complete data set is at least as recent as the deletion list or the differential
    # data:
    deletion_list_mtime     = os.path.getmtime(deletion_list)
    complete_data_mtime     = os.path.getmtime(complete_data)
    differential_data_mtime = os.path.getmtime(differential_data)
    if complete_data_mtime >= deletion_list_mtime or complete_data_mtime >= differential_data_mtime:
        util.SendEmail("Nichts zu tun!", "Komplettabzug ist neu.\n")
        sys.exit(0)

    timestamp = util.ReadTimestamp()
    if timestamp >= deletion_list_mtime or timestamp >= differential_data_mtime:
        util.SendEmail("Nichts zu tun!", "Keine neue Löschliste oder kein neuer Differenzabzug.\n")
        sys.exit(0)

    data_dir = PrepareDataDirectory() # After this we're in the data directory...

    util.ExtractAndRenameBSZFiles("../" + complete_data)
    util.ExtractAndRenameBSZFiles("../" + differential_data, "Diff")
    UpdateAllMarcFiles("../" + deletion_list) # ...and we're back in the original directory.

    util.WriteTimestamp()
    print("Successfully created updated MARC files.")


try:
    Main()
except Exception as e:
    util.SendEmail("Incremental File Update", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20))
