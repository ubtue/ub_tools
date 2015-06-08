#!/usr/bin/python

from email.mime.text import MIMEText
import ConfigParser
import os
import re
import smtplib
import sys
import traceback
import urllib2


def Error(msg):
    sys.stderr.write(sys.argv[0] + ": " + msg + "\n")
    os._exit(1)


def Warning(msg):
    sys.stderr.write(sys.argv[0] + ": " + msg + "\n")


def LoadConfigFile(path):
    try:
        if not os.access(path, os.R_OK):
            Error("can't open \"" + path + "\" for reading!")
        config = ConfigParser.ConfigParser()
        config.read(path)
        return config
    except:
        Error("failed to load the config file from \"" + path + "\"!")


def SendEmail(msg, subject, sender, recipient, server_address, server_user, server_password):
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
    except smtplib.SMTPException as e:
        Warning("Failed to send your email: " + str(e))
    server.quit()


DEFAULT_TIMEOUT = 20 # seconds


def RunTest(test_name, url, timeout, expected):
    if timeout is None:
        timeout = DEFAULT_TIMEOUT
    try:
        request = urllib2.Request(url, headers={"Accept-Language" : "de"})
        response = urllib2.urlopen(request, timeout=float(timeout))
        page_content = response.read()
        if expected is None:
            return True
        return re.match(expected, page_content, re.DOTALL) is not None
    except Exception as e:
        print "Caught an exception in RunTest()", str(e)
        return False


def Main():
    last_slash_pos = sys.argv[0].rfind("/")
    path = sys.argv[0][: last_slash_pos + 1]
    config = LoadConfigFile(path + "black_box_monitor.conf")

    notification_email_addr = config.get("Notify", "email")
    smtp_server = config.get("Notify", "smtp_server")
    email_server_user = config.get("Notify", "server_user")
    email_server_passwd = config.get("Notify", "server_passwd")

    for section in config.sections():
        if section == "Notify":
            continue
        url = config.get(section, "url")
        expected = None
        if config.has_option(section, "expected"):
            expected = config.get(section, "expected")
        timeout = None
        if config.has_option(section, "timeout"):
            timeout = config.get(section, "timeout")
        if not RunTest(section, url, timeout, expected):
            SendEmail("Test " + section + " failed!\n\n--Your friendly black box monitor", "Black Box Test Failed!",
                      "no_reply@uni-tuebingen.de", notification_email_addr, smtp_server, email_server_user,
                      email_server_passwd)


try:
    Main()
except Exception as e:
    print traceback.format_exc()
