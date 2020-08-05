# Python 3 module
# -*- coding: utf-8 -*-

from ftplib import FTP

class FTPConnection:

    _host = ""
    _username = ""
    _password = ""
    _ftp = None

    def __init__(self, host, username, password):
        self._host = host
        self._username = username
        self._password = password
        self._connect()
        self._login()

    def getFTP():
        return _ftp

    def _connect(self):
        try:
            self._ftp = FTP(host=self._host, timeout=120)
        except Exception as e:
            Error("failed to connect to FTP server! (" + str(e) + ")")

    def _login(self):
        try:
            self._ftp.login(user=self._username, passwd=self._password)
        except Exception as e:
            Error("failed to login to FTP server! (" + str(e) + ")")

    def changeDirectory(self, remote_dir_path):
        error_message = "failed to change directory: " + remote_dir_path
        try:
            res = self._ftp.cwd(remote_dir_path)
            if not res.startswith('250 '):
                Error(error_message)
        except Exception as e:
            Error(error_message + " (" + str(e) + ")")

    def uploadFile(self, local_file_path, remote_file_name):
        error_message = "failed to upload file " + local_file_path + " to " + remote_file_name
        try:
            with open(local_file_path, 'rb') as fp:
                res = self._ftp.storbinary("STOR " + remote_file_name, fp)
                if not res.startswith('226 '):
                    Error(error_message)
        except Exception as e:
            Error(error_message + " (" + str(e) + ")")

    def renameFile(self, remote_file_name_old, remote_file_name_new):
        error_message = "failed to rename file " + remote_file_name_old + " to " + remote_file_name_new
        try:
            res = self._ftp.rename(remote_file_name_old, remote_file_name_new)
            if not res.startswith('250 '):
                Error(error_message)
        except Exception as e:
            Error(error_message + " (" + str(e) + ")")
