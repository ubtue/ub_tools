# Python 2 module
# -*- coding: utf-8 -*-


from __future__ import print_function
from email.mime.text import MIMEText
import ConfigParser
import os
import smtplib
import socket
import struct
import sys


default_email_sender = "unset_email_sender@ub.uni-tuebingen.de"
default_email_recipient = "johannes.ruscheinski@ub.uni-tuebingen.de"


def SendEmail(subject, msg, sender=None, recipient=None):
    if sender is None:
        sender = default_email_sender
    if recipient is None:
        recipient = default_email_recipient
    config = LoadConfigFile(os.path.basename(sys.argv[0][:-2]) + "conf")
    try:
        server_address  = config.get("SMTPServer", "server_address")
        server_user     = config.get("SMTPServer", "server_user")
        server_password = config.get("SMTPServer", "server_password")
    except Exception as e:
        print("failed to read config file! (" + str(e) + ")", file=sys.stderr)
        sys.exit(-1)

    message = MIMEText(msg)
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
    SendEmail("Script error (" + os.path.basename(sys.argv[0]) + " on " + socket.gethostname() + ")!", msg)
    sys.exit(1)


def Warning(msg):
    print(sys.argv[0] + ": " + msg, file=sys.stderr)


# Fails if "source" does not exist or if "link_name" exists and is not a symlink.
# Calls Error() upon failure and aborts the program.
def SafeSymlink(source, link_name):
    try:
        os.lstat(source) # Throws an exception if "source" does not exist.
        if os.access(link_name, os.F_OK) != 0:
            if os.path.islink(link_name):
                os.unlink(link_name)
            else:
                Error("in SafeSymlink: trying to create a symlink to \"" + link_name
                      + "\" which is an existing non-symlink file!")
        os.symlink(source, link_name)
    except Exception as e:
        Error("os.symlink() failed: " + str(e))


def LoadConfigFile(path):
    try:
        if not os.access(path, os.R_OK):
            Error("can't open \"" + path + "\" for reading!")
        config = ConfigParser.ConfigParser()
        config.read(path)
        return config
    except Exception as e:
        Error("failed to load the config file from \"" + path + "\"! (" + str(e) + ")")


# This function looks for symlinks named "XXX-current-YYY" where "YYY" may be the empty string.  If found,
# it reads the creation time of the link target.  Next it looks for a file named "XXX-YYY.timestamp".  If
# the timestamp file does not exist, it assumes it found a new file and creates a new timestamp file with
# the creation time of the original link target and returns True.  If, on the other hand, a timestamp file
# was found, it reads a second timestamp from the timestamp file and compares it against the first
# timestamp.  If the timestamp from the timestamp file is older than the first timestamp, it updates the
# timestamp file with the newer timestamp and returns True, otherwise it returns False.
def FoundNewBSZDataFile(link_filename):
    try:
        statinfo = os.stat(link_filename)
    except FileNotFoundError as e:
        util.Error("Symlink \"" + link_filename + "\" is missing or dangling!")
    new_timestamp = statinfo.st_ctime
    timestamp_filename = sys.argv[0][:-2] + "timestamp"
    if not os.path.exists(timestamp_filename):
        with open(timestamp_filename, "wb") as timestamp_file:
            timestamp_file.write(struct.pack('d', new_timestamp))
        return True
    else:
        with open(timestamp_filename, "rb") as timestamp_file:
            (old_timestamp, ) = struct.unpack('d', timestamp_file.read())
        if (old_timestamp < new_timestamp):
            with open(timestamp_filename, "wb") as timestamp_file:
                timestamp_file.write(struct.pack('f', new_timestamp))
            return True
        else:
            return False
