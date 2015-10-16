# Python 2 module
# -*- coding: utf-8 -*-


from __future__ import print_function
from email.mime.text import MIMEText
import ConfigParser
import ctypes
import datetime
import glob
import os
import process_util
import re
import smtplib
import socket
import struct
import sys
import tarfile
import time


default_email_sender = "unset_email_sender@ub.uni-tuebingen.de"
default_email_recipient = "johannes.ruscheinski@uni-tuebingen.de"
default_config_file_dir = "/var/lib/tuelib/cronjobs/"


def SendEmail(subject, msg, sender=None, recipient=None):
    subject += " (from: " + socket.gethostname() + ")"
    if sender is None:
        sender = default_email_sender
    if recipient is None:
        recipient = default_email_recipient
    try:
        config = LoadConfigFile(no_error=True)
        server_address  = config.get("SMTPServer", "server_address")
        server_user     = config.get("SMTPServer", "server_user")
        server_password = config.get("SMTPServer", "server_password")
    except Exception as e:
        print("failed to read config file! (" + str(e) + ")", file=sys.stderr)
        sys.exit(-1)

    message = MIMEText(msg, 'plain', 'utf-8')
    message["Subject"] = subject
    message["From"] = sender
    message["To"] = recipient
    server = smtplib.SMTP(server_address)
    try:
        server.ehlo()
        server.starttls()
        server.login(server_user, server_password)
        server.sendmail(sender, [recipient], message.as_string())
    except Exception as e:
        print("Failed to send your email: " + str(e), file=sys.stderr)
        sys.exit(-1)
    server.quit()


def Error(msg):
    print(sys.argv[0] + ": " + msg, file=sys.stderr)
    SendEmail("Script error (script: " + os.path.basename(sys.argv[0]) + ")!", msg)
    sys.exit(1)


def Warning(msg):
    print(sys.argv[0] + ": " + msg, file=sys.stderr)


# @brief Copy the contents, in order, of "files" into "target".
# @return True if we succeeded, else False.
def ConcatenateFiles(files, target):
    if files is None or len(files) == 0:
        Error("\"files\" argument to util.ConcatenateFiles() is empty or None!")
    if target is None or len(target) == 0:
        Error("\"target\" argument to util.ConcatenateFiles() is empty or None!")
    return process_util.Exec("/bin/cat", files, new_stdout=target) == 0


# Fails if "source" does not exist or if "link_name" exists and is not a symlink.
# Calls Error() upon failure and aborts the program.
def SafeSymlink(source, link_name):
    try:
        os.lstat(source) # Throws an exception if "source" does not exist.
    except Exception as e:
        Error("in util.SafeSymlink: os.lstat(" + source + ") failed: " + str(e))
        
    if os.path.islink(link_name):
        os.unlink(link_name)
    elif os.path.isfile(link_name):
        Error("in util.SafeSymlink: trying to create a symlink to \"" + link_name
              + "\" which is an existing non-symlink file!")
    elif os.isdir(link_name):
        Error("in util.SafeSymlink: trying to create a symlink to \"" + link_name
              + "\" which is an existing non-symlink directory!")
    try:
        os.symlink(source, link_name)
    except Exception as e2:
        Error("in util.SafeSymlink: os.symlink(" + link_name + ") failed: " + str(e2))


# @return The absolute path of the file "link_name" points to.
def ResolveSymlink(link_name):
    resolved_path = os.readlink(link_name)
    if resolved_path[0] == '/':  # Absolute path?
        return resolved_path
    dirname = os.path.dirname(link_name)
    if not dirname:
        dirname = os.getcwdu()
    return os.path.join(dirname, resolved_path)


# @return True if "path" has been successfully unlinked, else False.
def Remove(path):
    try:
        os.unlink(path)
        return True
    except:
        return False


# @brief  Looks for "prefix.timestamp", and if found reads the timestamp stored in it.
# @param  prefix  This followed by ".timestamp" will be used as the name of the timestamp file.
#                 If this is None, the name of the Python executable minus the last three characters
#                 (hopefully ".py") will be used.
# @note   Timestamps are in seconds since the UNIX epoch.
# @return Either the value found in the timestamp file or the UNIX epoch, that is 0.
def ReadTimestamp(prefix = None):
    if prefix is None:
        prefix = os.path.basename(sys.argv[0])[:-3]
    timestamp_filename = prefix + ".timestamp"
    if not os.access(timestamp_filename, os.R_OK):
        return 0
    with open(timestamp_filename, "rb") as timestamp_file:
        (timestamp, ) = struct.unpack('d', timestamp_file.read())
        return timestamp


# @param timestamp  A float.
# @param prefix     This combined with ".timestamp" will be the name of the file that will be written.
# @param timestamp  A value in seconds since the UNIX epoch or None, in which case the current time
#                   will be used
def WriteTimestamp(prefix=None, timestamp=None):
    if prefix is None:
        prefix = os.path.basename(sys.argv[0])[:-3]
    elif type(timestamp) is not float:
        raise TypeError("timestamp argument of WriteTimestamp() must be of type \"float\"!")
    if timestamp is None:
        timestamp = time.time()
    timestamp_filename = prefix + ".timestamp"
    Remove(timestamp_filename)
    with open(timestamp_filename, "wb") as timestamp_file:
        timestamp_file.write(struct.pack('d', timestamp))


def LoadConfigFile(path=None, no_error=False):
    if path is None: # Take script name w/ "py" extension replaced by "conf".
        path = default_config_file_dir + os.path.basename(sys.argv[0])[:-2] + "conf"
    try:
        if not os.access(path, os.R_OK):
            if no_error:
                raise OSError("in util.LoadConfigFile: can't open \"" + path + "\" for reading!")
            Error("in util.LoadConfigFile: can't open \"" + path + "\" for reading!")
        config = ConfigParser.ConfigParser()
        config.read(path)
        return config
    except Exception as e:
        if no_error:
            raise e
        Error("in util.LoadConfigFile: failed to load the config file from \"" + path + "\"! (" + str(e) + ")")


# Extracts the typical files from a gzipped tar archive.
# @param name_prefix  If not None, this will be prepended to the names of the extracted files
# @return The list of names of the extracted files in the order: title data, superior data, norm data
def ExtractAndRenameBSZFiles(gzipped_tar_archive, name_prefix = None):
    # Ensures that all members of "gzipped_tar_archive" match our expectation as to what the BSZ should deliver.
    def TarFileMemberNamesAreOkOrDie(tar_file, archive_name):
        compiled_pattern = re.compile("^[STW]A-MARC-[a-z]+00\\d.raw$")
        for member in tar_file.getnames():
            if not compiled_pattern.match(member):
                Error("unknown tar file member \"" + member + "\" in \"" + archive_name + "\"!")


    # Extracts all tar file members matching "member_pattern" from "tar_file" and concatenates them as "new_name".
    def ExtractAndRenameMembers(tar_file, member_pattern, new_name):
        extracted_files = []
        compiled_pattern = re.compile(member_pattern)
        for member in tar_file.getnames():
            if compiled_pattern.match(member):
                tar_file.extract(member)
                extracted_files.append(member)

        Remove(new_name)
        ConcatenateFiles(extracted_files, new_name)

        # Clean up:
        for extracted_file in extracted_files:
            Remove(extracted_file)


    if name_prefix is None:
        name_prefix = ""
    tar_file = tarfile.open(gzipped_tar_archive, "r:gz")
    TarFileMemberNamesAreOkOrDie(tar_file, gzipped_tar_archive)
    current_date_str = datetime.datetime.now().strftime("%y%m%d")
    ExtractAndRenameMembers(tar_file, ".+a00\\d.raw$",
                            name_prefix + "TitelUndLokaldaten-" + current_date_str + ".mrc")
    ExtractAndRenameMembers(tar_file, ".+b00\\d.raw$",
                            name_prefix + "ÜbergeordneteTitelUndLokaldaten-" + current_date_str + ".mrc")
    ExtractAndRenameMembers(tar_file, ".+c00\\d.raw$", name_prefix + "Normdaten-" + current_date_str + ".mrc")

    return [name_prefix + "TitelUndLokaldaten-" + current_date_str + ".mrc",
            name_prefix + "ÜbergeordneteTitelUndLokaldaten-" + current_date_str + ".mrc",
            name_prefix + "Normdaten-" + current_date_str + ".mrc"]


def IsExecutableFile(executable_candidate):
    return os.path.isfile(executable_candidate) and os.access(executable_candidate, os.X_OK)


# @return the path to the executable "executable_candidate" if found, or else None
def Which(executable_candidate):

    dirname, basename = os.path.split(executable_candidate)
    if dirname:
        if IsExecutableFile(executable_candidate):
            return executable_candidate
    else:
        for path in os.environ["PATH"].split(":"):
            full_name = os.path.join(path, executable_candidate)
            if IsExecutableFile(full_name):
                return full_name

    return None


# Strips the path and an optional extension from "reference_file_name" and appends ".log"
# and prepends "log_directory".
# @return The complete path for the log file name.
def MakeLogFileName(reference_file_name, log_directory):
    if not log_directory.endswith("/"):
        log_directory += "/"
    last_dot_pos = reference_file_name.rfind(".")
    if last_dot_pos == -1:
        log_file_name = log_directory + os.path.basename(reference_file_name) + ".log"
    else:
        log_file_name = log_directory + os.path.basename(reference_file_name[:last_dot_pos]) + "log"


# @return the most recent file matching "file_name_glob" or None if there were no matching files.
def getMostRecentFileMatchingGlob(file_name_glob):
    most_recent_matching_name = None
    most_recent_mtime = None
    for name in glob.glob(file_name_glob):
        if not most_recent_matching_name:
            most_recent_matching_name = name
        else:
            stat_buf = os.stat(name)
            if most_recent_mtime is None or stat_buf.st_mtime > most_recent_mtime:
                most_recent_matching_name = name
                most_recent_mtime = stat_buf.st_mtime

    return most_recent_matching_name


# @brief Stores a list of files in a tarball
# @param tar_file_name       The name of the archive that will be created.  If the name ends with "gz" or "bz" the
#                            appropriate compression method will be applied.
# @param list_of_members     A list of pairs (2-tuples), one for each archive member.  The 0th slot of each pair
#                            specifies the file name that should be stored and the 1st slot the member name.
#                            If the member name is None, the file name from the 0th slot will be used.
# @param overwrite           If False, we fail if the tarball file already exists.
# @param delete_input_files  If True, after creating the tarball we delete the input files.
# @return None
def CreateTarball(tar_file_name, list_of_members, overwrite=False, delete_input_files=False):
    if not overwrite and os.access(tar_file_name, os.F_OK):
        Error("tarball \"" + tar_file_name + "\" already exists!")

    if tar_file_name.endswith("gz"):
        mode = "w:gz"
    elif tar_file_name.endswith("bz"):
        mode = "w:bz"
    else:
        mode = "w"
    
    new_tarfile = tarfile.open(name=tar_file_name, mode=mode)
    for file_and_member_names in list_of_members:
        new_tarfile.add(name=file_and_member_names[0], arcname=file_and_member_names[1])

    if not delete_input_files:
        return

    for file_and_member_names in list_of_members:
        if not Remove(file_and_member_names[0]):
            Error("in util.CreateTarball: can't delete \"" + file_and_member_names[0] + "\"!")


# @brief Deletes a symlink's target and the symlink itself
# @param link_name         The pathname of the symlink.
# @param fail_on_dangling  If True, abort the program if the symlink's link target does not exist. If this is False
#                          this function may still fail, for example, because the process lacks the permissions to
#                          delete the link target.
# @return None
def RemoveLinkTargetAndLink(link_name, fail_on_dangling=False):
    if not os.path.islink(link_name):
        Error("in util.RemoveLinkTargetAndLink: \"" + link_name + "\" is not a symlink!")
    try:
        link_target = os.readlink(link_name)
        ctypes.set_errno(0)
        os.unlink(link_target)
    except Exception as e:
        if not fail_on_dangling or ctypes.get_errno() != errno.ENOENT:
            Error("in util.RemoveLinkTargetAndLink: can't delete link target of \"" + link_name + "\"!")
    os.unlink(link_name)
