# Python 3 module
# -*- coding: utf-8 -*-

'''
Author  S. Lolong (steven.lolong@uni-tuebingen.de)
Year    2023

Most of the sftp client library used paramiko library.
Since, not all sftp client library features will be used is better to create own sftp client library with the essential features and use paramiko.
'''

from paramiko import (SSHClient, AutoAddPolicy)
from os import (path)
from abstract_ftp import AbstractFTP
import util

class SFTPConnection(AbstractFTP):  
  _host = ""
  _username = ""
  _password = ""
  _port = 22
  _ssh_client = None
  _sftp = None

  def __init__(self, hostname, username, password, port=None):
    self._host = hostname
    self._username = username
    self._password = password
    self._ssh_client = SSHClient()

    if port is not None:
      self._port = port
    
    self._connect()
    self._sftp =  self._ssh_client.open_sftp()

    
  def getFTP(self):
    return self._sftp
  

  def _connect(self):
    util.Info(f"Connecting to {self._host}")

    try:
      self._ssh_client.set_missing_host_key_policy(AutoAddPolicy())
      self._ssh_client.connect(hostname=self._host, port=self._port, username=self._username, password=self._password, look_for_keys=False, timeout=120)
    except Exception as excp:
      util.Error(f"Failed to connect to SFTP server! ({excp})")
    else:
      util.Info(f"Connected to server {self._host}")


  def changeDirectory(self, remote_dir_path):
    util.Info(f"change directory to: {remote_dir_path}")
    
    try:
      self._sftp.chdir(remote_dir_path)
    except Exception as excp:
      util.Error(f"failed to change directory: {remote_dir_path}! {excp}")
      

  def listDirectory(self, remote_dir_path='.'):
    try:
      return self._sftp.listdir(path=remote_dir_path)
    except Exception as excp:
      util.Error(f"failed to list directory ({excp})")


  def downloadFile(self, remote_file_path, local_file_path=None):
    if local_file_path is None:
      local_file_path = remote_file_path

    util.Info(f"downloading file from {self._host}:{remote_file_path} to {local_file_path}")
    
    try:
      self._sftp.get(remote_file_path, local_file_path)
    except Exception as excp:
      util.Error(f"File download failed! ({excp})")


  def uploadFile(self, local_file_path, remote_file_path=None):
    if remote_file_path is None:
      remote_file_path = path.basename(local_file_path)

    util.Info(f"uploading file {local_file_path} to {self._host}:{remote_file_path}")
    
    try:
      self._sftp.put(local_file_path, remote_file_path)
    except Exception as excp:
      util.Error(f"failed to upload file: {local_file_path}! ({excp})")

  
  def renameFile(self, remote_file_name_old, remote_file_name_new):
    try:
      self._sftp.rename(remote_file_name_old, remote_file_name_new)
    except IOError as excp:
      util.Error(f"failed to rename file {remote_file_name_old} to {remote_file_name_new}! ({excp})")
      

  def disconnect(self):
    self._ssh_client.close()
    util.Info(f"{self._username} is disconnected to server {self._host}:{self._port}")
   