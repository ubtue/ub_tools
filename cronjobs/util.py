# Python 3 module
# -*- coding: utf-8 -*-
from email.mime.application import MIMEApplication
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from typing import List
import configparser
import ctypes
import datetime
import email
import enum
import errno
import glob
import inspect
import mmap
import os
import process_util
import re
import smtplib
import socket
import struct
import sys
import tarfile
import time
import urllib.request


def WgetFetch(url: str, output: str = None) -> bool:
    if output == None:
        return True if process_util.Exec(Which("wget"), [ "--quiet", url ]) == 0 else False
    else:
        return True if process_util.Exec(Which("wget"), [ "--quiet", f"--output-document={output}", url ]) == 0 else False



def HTTPDateToSecondsRelativetoUnixEpoch(http_date: str) -> int:
    return email.utils.mktime_tz(email.utils.parsedate_tz(http_date))


class RetrieveFileByURLReturnCode(enum.Enum):
    SUCCESS = 0
    TIMEOUT = 1
    URL_NOT_FOUND = 2
    HTTP_ERROR = 3
    UNSPECIFIED_ERROR = 4
    BAD_CONTENT_TYPE = 5


# @brief Fetch a file given a URL
# @param url                    The URL to fetch.
# @param timeout                Give up if it takes longer than this many seconds to retrieve the file.
# @param accepted_content_types If non-empty, a list of acceptable content types.  If empty, anything will be accepted.
def RetrieveFileByURL(url: str, timeout: int, accepted_content_types: List[str] = []) -> RetrieveFileByURLReturnCode:
    deadline: int = time.time() + timeout
    attempt_no: int = 0
    while time.time() < deadline:
        try:
            headers = urllib.request.urlretrieve(url)[1]
            if accepted_content_types:
                content_type: str = headers["Content-type"].lower()
                for accepted_content_type in accepted_content_types:
                    if accepted_content_type.lower() == content_type:
                        return RetrieveFileByURLReturnCode.SUCCESS
                Warning("in RetrieveFileByURL: Content-type was \"" + content_type + "\"!")
                return RetrieveFileByURLReturnCode.BAD_CONTENT_TYPE
            return RetrieveFileByURLReturnCode.SUCCESS
        except urllib.error.URLError:
            return RetrieveFileByURLReturnCode.URL_NOT_FOUND
        except urllib.error.HTTPError as http_error:
            if http_error.code != 429:
                print("HTTP error reason: " + http_error.reason)
                print("HTTP headers: " + str(http_error.headers))
                return RetrieveFileByURLReturnCode.HTTP_ERROR
            if http_error.headers["Retry-After"]:
                timeout_or_date: str = http_error.headers["Retry-After"]
                try:
                    sleep_interval = int(http_error.headers["Retry-After"])
                except:
                    sleep_interval = HTTPDateToSecondsRelativetoUnixEpoch(http_error.headers["Retry-After"]) - time.time()
                if time.time() + sleep_interval > deadline:
                    return RetrieveFileByURLReturnCode.TIMEOUT
        except:
            return RetrieveFileByURLReturnCode.UNSPECIFIED_ERROR

        if 'sleep_interval' not in locals():
            sleep_interval: int = max(0, min(deadline - time.time(), 10 * 2 ** attempt_no))
        attempt_no += 1
        time.sleep(sleep_interval)
    return RetrieveFileByURLReturnCode.TIMEOUT


default_email_recipient = "johannes.riedl@uni-tuebingen.de"
default_config_file_dir = "/usr/local/var/lib/tuelib/cronjobs/"


# @param priority  The importance of the email.  Must be an integer from 1 to 5 with 1 being the lowest priority.
# @param attachment A path to the file that should be attached. Can be string or list of strings.
def SendEmailBase(subject: str, msg: str, sender: str = None, recipient: str = None, cc : str = None, priority: int = None,
              attachments: List[str] = None, log: bool = True):
    if recipient is None:
        recipient = default_email_recipient
    if priority is not None:
        if not isinstance(priority, int):
            Error("util.Sendmail called with a non-int priority!")
        if priority < 1 or priority > 5:
            Error("util.Sendmail called with a prioity that is not in [1..5]!")
    try:
        config = LoadConfigFile("/usr/local/var/lib/tuelib/cronjobs/smtp_server.conf", no_error=True)
        server_address  = config.get("SMTPServer", "server_address")
        server_user     = config.get("SMTPServer", "server_user")
        if sender is None:
            sender = "no-reply@ub.uni-tuebingen.de"
        server_password = config.get("SMTPServer", "server_password")
    except Exception as e:
        Info("failed to read config file! (" + str(e) + ")", file=sys.stderr)
        sys.exit(-1)

    message = MIMEMultipart()
    message["Subject"] = subject
    message["From"] = sender
    message["To"] = recipient
    if cc is not None:
        message["Cc"] = cc
    if priority is not None:
        message["X-Priority"] = str(priority)
    message.attach(MIMEText(msg, 'plain', 'utf-8'))

    if attachments is not None:
        if not isinstance(attachments, list):
            attachments = [ attachments ]

        for attachment in attachments:
            with open(attachment, "rb") as file:
                part = MIMEApplication(file.read(), Name=os.path.basename(attachment))
            part['Content-Disposition'] = 'attachment; filename="%s"' % os.path.basename(attachment)
            message.attach(part)

    server = smtplib.SMTP(server_address)
    try:
        server.ehlo()
        server.starttls()
        server.login(server_user, server_password)
        server.sendmail(sender, [recipient], message.as_string())
    except Exception as e:
        Info("Failed to send your email: " + str(e), file=sys.stderr)
        sys.exit(-1)
    server.quit()

    if log:
        message = "Sent email " + subject
        if sender is not None:
            message += ", sender: " + sender
        if recipient is not None:
            message += ", recipient: " + recipient
        if cc is not None:
            message += ", cc: " + cc
        Info(message)


# @param priority  The importance of the email.  Must be an integer from 1 to 5 with 1 being the lowest priority.
# @param attachment A path to the file that should be attached. Can be string or list of strings.
# extends default subject by script name and host name
def SendEmail(subject: str, msg: str, sender: str = None, recipient: str = None, cc : str = None, priority: int = None,
              attachments: List[str] = None, log: bool = True):
    subject = os.path.basename(sys.argv[0]) +  ": " + subject + " (from: " + socket.gethostname() + ")"
    SendEmailBase(subject, msg, sender, recipient, cc, priority, attachments, log)


# @param priority  The importance of the email.  Must be an integer from 1 to 5 with 1 being the lowest priority.
# @param attachment A path to the file that should be attached. Can be string or list of strings.
# @note Calls sys.exit with exit code -1 after sending the email
def SendEmailAndExit(subject: str, msg: str, sender: str = None, recipient: str = None, cc: str = None, priority: int = None,
                     attachments: List[str] = None, log: bool = True):
    SendEmail(subject, msg, sender, recipient, cc, priority, attachments, log)
    sys.exit(-1)


def Error(msg):
    msg = os.path.basename(inspect.stack()[1][1]) + "." + inspect.stack()[1][3] + ": " + msg
    Info(sys.argv[0] + ": " + msg, file=sys.stderr)
    SendEmail("Script error (script: " + os.path.basename(sys.argv[0]) + ")!", msg, priority=1)
    sys.exit(1)


def Warning(msg):
    msg = os.path.basename(inspect.stack()[1][1]) + "." + inspect.stack()[1][3] + ": " + msg
    Info(sys.argv[0] + ": " + msg, file=sys.stderr)


def Info(*args, file=sys.stdout):
    for arg in args:
        print(arg, file=file, end='')
    print(file=file, flush=True)


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
    elif os.path.isdir(link_name):
        Error("in util.SafeSymlink: trying to create a symlink to \"" + link_name
              + "\" which is an existing non-symlink directory!")
    try:
        os.symlink(source, link_name)
    except Exception as e2:
        Error("in util.SafeSymlink: os.symlink(" + link_name + ") failed: " + str(e2))


# @return The absolute path of the file "link_name" points to.
def ResolveSymlink(link_name):
    assert(os.path.islink(link_name))
    resolved_path = os.path.normpath(os.readlink(link_name))
    if os.path.isabs(resolved_path):
        return resolved_path
    return os.path.join(os.path.dirname(link_name), resolved_path)


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
    elif not isinstance(timestamp, float):
        raise TypeError("timestamp argument of WriteTimestamp() must be of type \"float\"!")
    if timestamp is None:
        timestamp = time.time()
    timestamp_filename = prefix + ".timestamp"
    Remove(timestamp_filename)
    with open(timestamp_filename, "wb") as timestamp_file:
        timestamp_file.write(struct.pack('d', timestamp))


def LoadConfigFile(path=None, no_error=False):
    if path is None: # Take script name w/ "py" extension replaced by "conf".
        # Check whether there is a machine specific subdirectory
        hostname_dir = default_config_file_dir + socket.gethostname() + "/"
        path = hostname_dir + os.path.basename(sys.argv[0])[:-2] + "conf"
        if not os.access(path, os.R_OK):
            path = default_config_file_dir + os.path.basename(sys.argv[0])[:-2] + "conf"
    try:
        if not os.access(path, os.R_OK):
            if no_error:
                raise OSError("in util.LoadConfigFile: can't open \"" + path + "\" for reading!")
            Error("in util.LoadConfigFile: can't open \"" + path + "\" for reading!")
        config = configparser.ConfigParser()
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
        compiled_pattern = re.compile("(aut|tit).mrc$")
        for member in tar_file.getnames():
            if not compiled_pattern.search(member):
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
    ExtractAndRenameMembers(tar_file, "^tit.mrc$",
                            name_prefix + "GesamtTiteldaten-" + current_date_str + ".mrc")
    ExtractAndRenameMembers(tar_file, "^aut.mrc$", name_prefix + "Normdaten-" + current_date_str + ".mrc")

    return [name_prefix + "GesamtTiteldaten-" + current_date_str + ".mrc",
            name_prefix + "Normdaten-" + current_date_str + ".mrc"]


def IsExecutableFile(executable_candidate):
    return os.path.isfile(executable_candidate) and os.access(executable_candidate, os.X_OK)


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
        log_file_name = log_directory + os.path.basename(reference_file_name[:last_dot_pos]) + ".log"

    return log_file_name

# @return the most recent file matching "file_name_glob" or None if there were no matching files.
def getMostRecentFileMatchingGlob(file_name_glob):
    most_recent_matching_name = None
    most_recent_mtime = None
    for name in glob.glob(file_name_glob):
        if not most_recent_matching_name:
            most_recent_matching_name = name
            stat_buf = os.stat(name)
            most_recent_mtime = stat_buf.st_mtime
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
        new_tarfile.add(name=os.path.realpath(file_and_member_names[0]), arcname=file_and_member_names[1])

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
    except Exception:
        if not fail_on_dangling or ctypes.get_errno() != errno.ENOENT:
            Error("in util.RemoveLinkTargetAndLink: can't delete link target of \"" + link_name + "\"!")
    os.unlink(link_name)


def GetLogDirectory():
    if os.access("/usr/local/var/log/tuefind", os.F_OK):
        return "/usr/local/var/log/tuefind"
    else:
        Warning("Can't find the log directory!  Logging to /tmp instead.")
        return "/tmp"


# Create an empty file, or, if it exists set its access and modification time.
# If "times" is None, the current time will be used, o/w "times" must be a 2-tuple
# of the form (atime, mtime).
def Touch(filename, times=None):
    with open(filename, "a"):
        os.utime(filename, times)


def ExecOrDie(cmd_name, args, log_file_name=None, setsid=True):
    if log_file_name is None:
        log_file_name = "/proc/self/fd/2" # stderr
    if not process_util.Exec(cmd_path=cmd_name, args=args, new_stdout=log_file_name,
                             new_stderr=log_file_name, append_stdout=True, append_stderr=True, setsid=setsid) == 0:
        SendEmail("util.ExecOrDie", "Failed to execute \"" + cmd_name + "\".\nSee logfile \"" + log_file_name
                  + "\" for the reason.", priority=1)
        sys.exit(-1)


# @brief Looks for "executable_name" in $PATH unless it contains a slash.
# @return Either the path to an executable program or the empty string.
def Which(executable_name):
    if '/' in executable_name:
        return executable_name if os.access(executable_name, os.X_OK) else ""
    path = os.getenv("PATH")
    if path is None:
        return ""
    for path_component in path.split(':'):
        if os.access(path_component + "/" + executable_name, os.X_OK):
            return path_component + "/" + executable_name
    return ""


# Returns the last "max_no_of_lines" of "filename" or the contents of the entire file if the file contains no more than
# "max_no_of_lines".
# @return The requested lines.
def Tail(filename, max_no_of_lines):
    with open(filename, "rb") as f:
        map = mmap.mmap(f.fileno(), 0, prot=mmap.PROT_READ)

        map.seek(0, os.SEEK_END)
        no_of_lines = 0
        requested_lines = bytearray()
        while map.tell() != 0:
            cur_pos = map.tell()
            map.seek(cur_pos - 1)
            previous_byte = map.read_byte()
            if previous_byte == '\n':
                no_of_lines += 1
                if no_of_lines == max_no_of_lines + 1:
                    return str(requested_lines[::-1])
            map.seek(cur_pos - 1)
            requested_lines += previous_byte

        map.close()
        return str(requested_lines[::-1])


def RenameFile(old_path : str, new_path : str) -> None:
    ExecOrDie("/bin/mv", [ "--force", old_path, new_path ])
