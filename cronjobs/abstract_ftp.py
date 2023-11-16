from abc import ABC, abstractclassmethod, abstractproperty

class AbstractFTP(ABC):

  @abstractclassmethod
  def __init__(self, hostname, username, password, port=None):
    pass

  @abstractclassmethod
  def getFTP(self):
    pass

  @abstractclassmethod
  def _connect(self):
    pass

  @abstractclassmethod
  def changeDirectory(self, remote_dir_path):
    pass
  
  @abstractclassmethod
  def listDirectory(self, remote_dir_path='.'):
    pass

  @abstractclassmethod
  def downloadFile(self, remote_file_name, local_file_path=None):
    pass

  @abstractclassmethod  
  def uploadFile(self, local_file_path, remote_file_path=None):
    pass

  @abstractclassmethod
  def renameFile(self, remote_file_name_old, remote_file_name_new):
    pass

  @abstractclassmethod
  def disconnect(self):
    pass