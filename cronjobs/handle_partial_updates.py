#!/bin/python3
# -*- coding: utf-8 -*-
#
# A tool that creates a pseudo complete MARC data download from the BSZ from a differential download
# a deletion list and an old complete MARC download.
# A typical config file looks like this:
"""
[Files]
loesch_liste    = LOEKXP-current
komplett_abzug  = WA-MARC-krimdok-current.tar.gz
differenz_abzug = SA-MARC-krimdok-current.tar.gz

[SMTPServer]
server_address  = smtpserv.uni-tuebingen.de
server_user     = XXXXXX
server_password = XXXXXX
"""

import datetime
import glob
import os
import process_util
import re
import shutil
import sys
import tarfile
import traceback
import util


def EnsureFileIsEmptyOrEndsWithNewline(filename):
    with open(filename, "rb+") as file:
        try:
            file.seek(-1, 2)
            if file.read(1) != "\n":
                file.seek(0, 2)
                file.write("\n")
        except:
            pass # File was probably empty


# Create a new deletion list "augmented_list" which consists of a copy of "orig_list" and the
# list of extracted control numbers from "changed_marc_data".
def AugmentDeletionList(orig_list, changed_marc_data, augmented_list):
    util.Remove(augmented_list)
    shutil.copyfile(orig_list, augmented_list)
    if process_util.Exec("extract_IDs_in_erase_format.sh", args=[changed_marc_data, augmented_list],
                         timeout=100) != 0:
        util.Error("failed to create \"" + augmented_list + "\" from \"" + changed_marc_data + "\"!")
    util.Info("Successfully created \"" + augmented_list + "\".")


def DeleteMarcRecords(original_marc_file, deletion_list, processed_marc_file):
    util.Remove(processed_marc_file)
    if process_util.Exec("delete_ids", args=[deletion_list, original_marc_file, processed_marc_file],
                         timeout=200) != 0:
        util.Error("failed to create \"" + processed_marc_file + "\" from \"" + deletion_list + "\" and \""
                   + original_marc_file + "\"!")
    util.Info("Successfully created \"" + processed_marc_file + "\".")


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
# successfully, the three files "Normdaten-YYMMDD.mrc", "GesamtTiteldaten-YYMMDD.mrc",
# will be left in our data directory.  Here YYMMDD is the current date.
# Also changes into the parent directory.
# Returns a tuple of the updated file names in the order (titel_data, superior_data, norm_data).
def UpdateAllMarcFiles(orig_deletion_list):
    # Create a deletion list that consists of the original list from the
    # BSZ as well as all the ID's from the files starting w/ "Diff":
    util.Remove("augmented_deletion_list")
    if orig_deletion_list is None: # Create empty file.
        with open("augmented_deletion_list", "a") as _:
            pass
    else:
        shutil.copyfile("../" + orig_deletion_list, "augmented_deletion_list")
        EnsureFileIsEmptyOrEndsWithNewline("augmented_deletion_list")
    extract_IDs_script_path = GetPathOrDie("extract_IDs_in_erase_format.sh")
    for marc_file_name in glob.glob("*.mrc"):
        if not marc_file_name.startswith("Diff"):
            continue
        if process_util.Exec(extract_IDs_script_path,
                             args=[marc_file_name, "augmented_deletion_list"],
                             timeout=100) != 0:
            util.Error("failed to append ID's from \"" + marc_file_name
                       + "\" to \"augmented_deletion_list\"!")
    util.Info("Created an augmented deletion list.")

    # Now delete ID's from the augmented deletion list from all MARC-21 files:
    delete_ids_path = GetPathOrDie("delete_ids")
    for marc_file_name in glob.glob("*.mrc"):
        if marc_file_name.startswith("Diff"):
            continue
        trimmed_marc_file = marc_file_name[:-4] + "-trimmed.mrc"
        if process_util.Exec(delete_ids_path, args=["augmented_deletion_list", marc_file_name, trimmed_marc_file],
                             timeout=200, new_stdout=util.GetLogDirectory() + "/trimmed_marc.log",
                             new_stderr=util.GetLogDirectory() + "/trimmed_marc.log") != 0:
            util.Error("failed to create \"" + trimmed_marc_file + " from \"augmented_deletion_list\" and "
                       "\"" + marc_file_name + "\"!")
        RemoveOrDie(marc_file_name)
    RemoveOrDie("augmented_deletion_list")
    util.Info("Deleted ID's from MARC files.")

    # Now concatenate the changed MARC records with the trimmed data sets:
    for marc_file_name in glob.glob("*-trimmed.mrc"):
        root_name = marc_file_name[:-19]
        diff_name = glob.glob("Diff" + root_name + "*.mrc")[0]
        if not util.ConcatenateFiles([marc_file_name, diff_name], root_name + ".mrc"):
            util.Error("We failed to concatenate \"" + marc_file_name + "\" and \"" + diff_name + "\"!")
        RemoveOrDie(marc_file_name)
        RemoveOrDie(diff_name)
    util.Info("Created concatenated MARC files.")

    # Rename files to include the current date and move them up a directory:
    current_date_str = datetime.datetime.now().strftime("%y%m%d")
    marc_files = glob.glob("*.mrc")
    for marc_file_name in marc_files:
        RenameOrDie(marc_file_name, "../" + marc_file_name[:-4] + "-" + current_date_str + ".mrc")
    os.chdir("..")
    util.Info("Renamed and moved files.")

    # Create symlinks with "current" instead of "YYMMDD" in the orginal files:
    for marc_file in marc_files:
        new_name = marc_file[:-4] + "-" + current_date_str + ".mrc"
        util.SafeSymlink(new_name, re.sub("\\d\\d\\d\\d\\d\\d", "current", new_name))
    util.Info("Symlinked files.")
    return ("GesamtTiteldaten-current.mrc", "Normdaten-current.mrc")


# Creates a new tarball named "new_tar_file_name" and linked from "link_name".
# Archive member names are constructed to resemble the names found in the archive initially linked
# from "link_name" and are assumed to end in "a001.raw" etc.  Therefore "link_name" must initially
# point to an existing tarball.  This pre-existing tarball will be deleted and "link_name" will
# end up pointing to the new tarball.
def CreateNewTarballAndDeletePreviousTarball(new_tar_file_name, title_superior_norm_tuple, link_name):
    # Determine the base name for the archive members:
    tar_file = tarfile.open(name=link_name, mode="r:*")
    base_name = tar_file.getnames()[0][-1000:-8]

    file_and_member_names = []
    file_and_member_names.append((title_superior_norm_tuple[0], base_name + "a001.raw"))
    file_and_member_names.append((title_superior_norm_tuple[1], base_name + "b001.raw"))
    file_and_member_names.append((title_superior_norm_tuple[2], base_name + "c001.raw"))
    util.CreateTarball(new_tar_file_name, file_and_member_names)

    util.RemoveLinkTargetAndLink(link_name)
    try:
        os.symlink(new_tar_file_name, link_name)
    except Exception as e2:
        util.Error("in CreateNewTarballAndDeletePreviousTarball: os.symlink(" + link_name + ") failed: " + str(e2))


def Main():
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
    if not os.access(complete_data, os.R_OK):
        util.Error("Fehlender oder nicht lesbarer Komplettabzug. (" + complete_data + ")")
    deletion_list_is_readable = os.access(deletion_list, os.R_OK)
    if not deletion_list_is_readable:
        deletion_list = None
    differential_data_is_readable = os.access(differential_data, os.R_OK)
    if not deletion_list_is_readable and not differential_data_is_readable:
        util.Error("Fehlende oder nicht lesbare Löschliste und Differenzabzug..")

    # Bail out if the most recent complete data set is at least as recent as the deletion list or the differential
    # data:
    complete_data_mtime = os.path.getmtime(complete_data)
    deletion_list_mtime = None
    if deletion_list_is_readable:
        deletion_list_mtime = os.path.getmtime(deletion_list)
    differential_data_mtime = None
    if differential_data_is_readable:
        differential_data_mtime = os.path.getmtime(differential_data)
    if ((deletion_list_mtime is not None and complete_data_mtime >= deletion_list_mtime)
        or (differential_data_mtime is not None and complete_data_mtime >= differential_data_mtime)):
        util.SendEmail("Nichts zu tun!", "Komplettabzug ist neuer als eventuell vorhandene Differenzabzüge.\n", priority=5)
        sys.exit(0)

    PrepareDataDirectory() # After this we're in the data directory...

    util.ExtractAndRenameBSZFiles("../" + complete_data)
    util.ExtractAndRenameBSZFiles("../" + differential_data, "Diff")
    title_superior_norm_tuple = UpdateAllMarcFiles(deletion_list) # ...and we're back in the original directory.

    new_tarball_name = complete_data.replace("current", datetime.date.today().strftime("%y%m%d"))
    CreateNewTarballAndDeletePreviousTarball(new_tarball_name, title_superior_norm_tuple,
                                             complete_data)
    util.RemoveLinkTargetAndLink(title_superior_norm_tuple[0])
    util.RemoveLinkTargetAndLink(title_superior_norm_tuple[1])
    util.RemoveLinkTargetAndLink(title_superior_norm_tuple[2])
    util.Info("Successfully created updated MARC files.")


try:
    Main()
except Exception as e:
    util.SendEmail("Incremental File Update", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
