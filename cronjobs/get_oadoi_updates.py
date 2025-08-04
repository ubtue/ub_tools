#!/bin/python3
# -*- coding: utf-8 -*-
# Tool for downloading OADOI update files
# Default config file
"""
[Unpaywall]
changelist_url = http://api.unpaywall.org/feed/changefile
api_key = MY_API_KEY
local_update_file_dir = /tmp/oadoi
changelist_file_regex = changed_dois_with_versions_([\\d-]+)(.*)([\\d-]).*.jsonl.gz
"""

import json
import os
import re
import subprocess
import sys
import traceback
import urllib.request, urllib.parse, urllib.error
import util


def GetRemoteUpdateObjects(url, api_key):
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
    not_in_both = list(set(remote_update_list) - set(local_update_list))
    # Get the oldest locally missing or None
    oldest_missing_remote = next(iter(sorted(not_in_both) or []), None)
    if oldest_missing_remote is None:
        # We alread have all files locally
        return []
    else:
        download_list = [item for item in not_in_both if item >= oldest_missing_remote]
        update_list = [item for item in remote_update_list if item >= oldest_missing_remote]
        return { "download" :  sorted(download_list), "update" : sorted(update_list) }


def GetDownloadUrls(download_list, json_update_objects, api_key):
    download_urls = [item['url'] for item in json_update_objects if item['filename'] in download_list]
    return list([url.replace("YOUR_API_KEY", api_key) for url in download_urls])


def DownloadUpdateFiles(download_list, json_update_objects, api_key, target_directory=None):
    download_urls = GetDownloadUrls(download_list, json_update_objects, api_key)
    if not target_directory is None:
       os.chdir(target_directory)

    oadoi_downloader = urllib.request.URLopener()
    for url in download_urls:
        # XXX CHANGE ME TO ACTIVE
        print("WE WOULD RETRIEVE: " + oadoi_downloader.retrieve(" + url + "))


def UpdateDatabase(update_list, config, source_directory=None):
    database = config.get("MongoDB", "database")
    collection = config.get("MongoDB", "collection")
    host = config.get("MongoDB", "host")
    if not source_directory is None:
        os.chdir(source_directory)
    for filename in update_list:
         #Pipe through zcat so we don't have to explicitly unpack
         zcat = subprocess.Popen( [ "zcat", filename ], stdout=subprocess.PIPE)
         # XXX Notice that --upsert must be --mode=upsert with MongoDB 3.6 client on CentOS7
         mongo = subprocess.Popen( ["mongoimport",
                                   "--db", database, "--collection", collection, "--upsert", "--upsertFields", "doi", "--host", host ],
                                   stdin=zcat.stdout, stdout=sys.stdout)
         zcat.stdout.close() # Allow p1 to receive a SIGPIPE if p2 exits.
         returncode = zcat.wait()
         if returncode != 0:
             sys.exit(1)
         mongo.communicate()


def Main():
    config = util.LoadConfigFile()
    changelist_url = config.get("Unpaywall", "changelist_url")
    api_key = config.get("Unpaywall", "api_key")
    working_dir = config.get("LocalConfig", "working_dir")
    json_update_objects = GetRemoteUpdateObjects(changelist_url, api_key)
    remote_update_files = GetRemoteUpdateFiles(json_update_objects)
    local_update_files = GetLocalUpdateFiles(config, working_dir)
    update_and_download_lists = GetAllFilesFromLastMissingLocal(remote_update_files, local_update_files)
    if not update_and_download_lists:
        print("Received empty list - so nothing to do")
        sys.exit(0)
    DownloadUpdateFiles(update_and_download_lists['download'], json_update_objects, api_key, working_dir)
    UpdateDatabase(update_and_download_lists['update'], config)


try:
    Main()
except Exception as e:
    error_msg = "An unexpected error occured: " + str(e) + "\n\n" + traceback.format_exc(20)
    sys.stderr.write(error_msg)
