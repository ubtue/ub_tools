#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Tool for downloading OADOI update files
# Default config file
"""
[Unpaywall]
changelist_url = http://api.unpaywall.org/feed/changefile
api_key = MY_API_KEY
changelist_file_regex = changed_dois_with_versions_([\d-]+)(.*)([\d-]).*.jsonl.gz
"""


import json
import os
import process_util
import re
import sys
import traceback
import urllib.request, urllib.parse, urllib.error
import util


def GetRemoteUpdateObjects(url, api_key):
    print("Get Changelists")
    response = urllib.request.urlopen(url + '?api_key=' + api_key)
    jdata = json.load(response)
    # Get only JSON update entries, no CSV
    json_update_objects = [item for item in jdata['list'] if item['filetype'] == 'jsonl']
    return json_update_objects


def GetRemoteUpdateFiles(json_update_objects):
    json_filenames = [item['filename'] for item in json_update_objects]
    return json_filenames


def GetLocalUpdateFiles(config, local_directory=None):
    def GetDirectoryContents():
        if local_directory is None:
            return os.listdir(".")
        else:
            return os.listdir(local_directory)
    changelist_file_regex = re.compile(config.get("Unpaywall", "changelist_file_regex"))
    return list(filter(changelist_file_regex.search, GetDirectoryContents()))


def GetAllFilesFromLastMissingLocal(remote_update_list, local_update_list):
    # Strategy: Determine the youngest local file such that all previous files
    # are already locally present and return all younger remote files
    youngest_local = next(iter(sorted(local_update_list, reverse=True)))
    download_list = [item for item in remote_update_list if item > youngest_local]
    return { "download" :  sorted(download_list) }


def GetDownloadUrlsAndFilenames(download_list, json_update_objects, api_key):
    download_urls = [item['url'] for item in json_update_objects if item['filename'] in download_list]
    filenames = [item['filename'] for item in json_update_objects if item['filename'] in download_list]
    return { "urls": sorted(download_urls), "filenames" : sorted(filenames) }


def GetImportFiles(config, oadoi_download_directory, oadoi_imported_directory):
    downloaded_files =  GetLocalUpdateFiles(config, oadoi_download_directory)
    imported_files = GetLocalUpdateFiles(config, oadoi_imported_directory)
    return list(set(downloaded_files) - set(imported_files))


def DownloadUpdateFiles(download_list, json_update_objects, api_key, target_directory=None):
    download_urls_and_filenames = GetDownloadUrlsAndFilenames(download_list, json_update_objects, api_key)
    if not target_directory is None:
       os.chdir(target_directory)
       
    oadoi_downloader = urllib.request.URLopener()
    for url, filename in zip(download_urls_and_filenames['urls'], download_urls_and_filenames['filenames']):
        print("Downloading \"" + url + "\" to \"" + filename + "\"")
        oadoi_downloader.retrieve(url, filename)
        

def CreateImportedSymlink(filename, dest):
    print("Creating symlink in imported directory")
    os.symlink(os.getcwd() + "/" + filename, dest)


def UpdateDatabase(update_list, source_directory=None):
    if not source_directory is None:
       os.chdir(source_directory)
    imported_symlinks_directory = os.getcwd() + "/imported"
    for filename in update_list:
        imported_symlink_full_path = imported_symlinks_directory + "/" + filename
        if os.path.islink(imported_symlink_full_path):
            print(("Skipping " + filename + " since apparently already imported"))
            continue
        print("Importing \"" + filename + "\"")
        util.ExecOrDie(util.Which("import_oadois_to_mongo.sh"), filename)
        CreateImportedSymlink(imported_symlink_full_path)


def Main():
    config = util.LoadConfigFile()
    changelist_url = config.get("Unpaywall", "changelist_url")
    api_key = config.get("Unpaywall", "api_key")
    oadoi_download_directory = config.get("LocalConfig", "download_dir") 
    oadoi_imported_directory = oadoi_download_directory + "/imported/"
    json_update_objects = GetRemoteUpdateObjects(changelist_url, api_key)
    remote_update_files = GetRemoteUpdateFiles(json_update_objects)
    local_update_files = GetLocalUpdateFiles(config, oadoi_download_directory)
    download_lists = GetAllFilesFromLastMissingLocal(remote_update_files, local_update_files)
    DownloadUpdateFiles(download_lists['download'], json_update_objects, api_key, oadoi_download_directory)
    UpdateDatabase(GetImportFiles(config, oadoi_download_directory, oadoi_imported_directory), oadoi_download_directory)


try:
    Main()
except Exception as e:
    error_msg = "An unexpected error occured: " + str(e) + "\n\n" + traceback.format_exc(20)
    sys.stderr.write(error_msg)
