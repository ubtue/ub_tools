# Python 3 module
# -*- coding: utf-8 -*-
import paramiko
import os
import util

class SFTPConnection:  
  _host = ""
  _username = ""
  _password = ""
  _port = 22
  _sftp = None

  def __init__(self, hostname, username, password, port=None):
    self._host = hostname
    self._username = username
    self._password = password
    self._sftp = paramiko.SSHClient()

    if port is not None:
      self._port = port
    
    self._connect()

    
  def getSFTP(self):
    return self._sftp
  

  def _connect(self):
    util.Info(f"Connecting to {self._host}")
    try:
      self._sftp.set_missing_host_key_policy(paramiko.AutoAddPolicy())
      self._sftp.connect(hostname=self._host, port=self._port, username=self._username, password=self._password, look_for_keys=False, timeout=120)
    except Exception as excp:
      util.Error(f"Failed to connect to SFTP server! ({excp})")
    else:
      util.Info(f"Connected to server {self._host}")


  def changeDirectory(self, remote_dir_path):
    util.Info(f"change directory to: {remote_dir_path}")
    sftp_client = self._sftp.open_sftp()
    try:
      sftp_client.chdir(remote_dir_path)
    except Exception as excp:
      util.Error(f"failed to change directory: {remote_dir_path}! {excp}")
      

  def listDirectory(self, remote_dir_path='.'):
    sftp_client = self._sftp.open_sftp()
    try:
      return sftp_client.listdir(path=remote_dir_path)
    except Exception as excp:
      util.Error(f"failed to list directory ({excp})")


  def downloadFile(self, remote_file_path, local_file_path=None):
    if local_file_path is None:
      local_file_path = remote_file_path

    util.Info(f"downloading file from {remote_file_path} to {local_file_path}")
    sftp_client = self._sftp.open_sftp()
    try:
      sftp_client.get(remote_file_path, local_file_path)
    except Exception as excp:
      util.Error(f"File download failed! ({excp})")


  def uploadFile(self, local_file_path, remote_file_path=None):
    if remote_file_path is None:
      remote_file_path = os.path.basename(local_file_path)

    util.Info(f"uploading file {local_file_path} to {remote_file_path}")
    sftp_client = self._sftp.open_sftp()
    try:
      sftp_client.put(local_file_path, remote_file_path)
    except Exception as excp:
      util.Error(f"failed to upload file: {local_file_path}! ({excp})")

    sftp_client.close()

  
  def renameFile(self, remote_file_name_old, remote_file_name_new):
    sftp_client = self._sftp.open_sftp()

    try:
      sftp_client.rename(remote_file_name_old, remote_file_name_new)
    except Exception as excp:
      util.Error(f"failed to rename file {remote_file_name_old} to {remote_file_name_new}! ({excp})")
      

  def disconnect(self):
    self._sftp.close()
    util.Info(f"{self._username} is disconnected to server {self._host}:{self._port}")
   
     
  def executeCommand(self, command):
    stdout,stderr= self._sftp.exec_command(command)
    util.Info(stdout.readlines())
    util.Info(stderr.readlines())
