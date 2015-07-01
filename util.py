# Python 3 module


from email.mime.text import MIMEText
import configparser
import os
import smtplib
import sys


def SendEmail(subject, msg, sender="fetch_marc_updates@ub.uni-tuebingen.de",
              recipient="johannes.ruscheinski@uni-tuebingen.de"):
    config = LoadConfigFile(sys.argv[0][:-2] + "conf")
    try:
        server_address  = config["SMTPServer"]["server_address"]
        server_user     = config["SMTPServer"]["server_user"]
        server_password = config["SMTPServer"]["server_password"]
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
    SendEmail("SWB FTP Failed!", msg)
    sys.exit(1)


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
        config = configparser.ConfigParser()
        config.read(path)
        return config
    except Exception as e:
        Error("failed to load the config file from \"" + path + "\"! (" + str(e) + ")")
