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

import dbus
import json
import os
import distro
import re
import sys
import time
import traceback
import urllib.request, urllib.parse, urllib.error
import util
from shutil import copy2, move, rmtree

def GetChangelists(url, api_key):
    print("Get Changelists")
    for attempt_number in range(3):
        try:
            response = urllib.request.urlopen(url + '?api_key=' + api_key)
            jdata = json.load(response)
            # Get only JSON update entries, no CSV
            json_update_objects = [item for item in jdata['list'] if item['filetype'] == 'jsonl']
            return json_update_objects
        except Exception as e:
            exception = e
            time.sleep(10 * (attempt_number + 1))
    raise exception


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


def GetAllFilesStartingAtFirstMissingLocal(remote_update_list, local_update_list):
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


def ImportOADOIsToMongo(update_list, source_directory=None, log_file_name="/dev/stderr"):
    if not source_directory is None:
       os.chdir(source_directory)
    imported_symlinks_directory = os.getcwd() + "/imported"
    for filename in update_list:
        imported_symlink_full_path = imported_symlinks_directory + "/" + filename
        if os.path.islink(imported_symlink_full_path):
            print("Skipping " + filename + " since apparently already imported")
            continue
        print("Importing \"" + filename + "\"")
        util.ExecOrDie(util.Which("import_oadois_to_mongo.sh"), [ filename ], log_file_name)
        CreateImportedSymlink(filename, imported_symlink_full_path)


def ExtractOADOIURLs(share_directory, all_dois_file, urls_file, log_file_name):
    print("Extract URLs for DOI's in " + all_dois_file)
    util.ExecOrDie(util.Which("extract_oadoi_urls.sh"), [ share_directory + '/' + all_dois_file, urls_file ], log_file_name)


def ShareOADOIURLs(share_directory, urls_file):
    copy2(urls_file, share_directory)


def GetMongoServiceDependingOnSystem():
    distro = distro.id()
    if distro.id() == 'ubuntu':
        return 'mongod.service'
    util.SendEmail("Update OADOI Data", "Cannot handle Distro \"" + distro.id() + "\"", priority=1);
    sys.exit(-1)


def GetSystemDDBusManager():
    sysbus = dbus.SystemBus()
    systemd1 = sysbus.get_object('org.freedesktop.systemd1', '/org/freedesktop/systemd1')
    manager = dbus.Interface(systemd1, 'org.freedesktop.systemd1.Manager')
    return manager


def StartMongoDB():
    manager = GetSystemDDBusManager()
    manager.RestartUnit(GetMongoServiceDependingOnSystem(), 'fail')


def StopMongoDB():
    manager = GetSystemDDBusManager()
    manager.StopUnit(GetMongoServiceDependingOnSystem(), 'ignore-dependencies')


def DumpMongoDB(config, log_file_name="/dev/stderr"):
    # Backup to intermediate hidden directory that is exluded from backup
    # to prevent inconsistent saving
    dump_base_path = config.get("LocalConfig", "dump_base_path")
    dump_root = config.get("LocalConfig", "dump_root")
    intermediate_dump_dir = dump_base_path + '/.' + dump_root
    util.ExecOrDie(util.Which("mongodump"), [ "--out=" + intermediate_dump_dir , "--gzip" ], log_file_name)
    final_dump_dir = dump_base_path + '/' + dump_root
    if os.path.exists(final_dump_dir) and os.path.isdir(final_dump_dir):
        rmtree(final_dump_dir)
    move(intermediate_dump_dir, final_dump_dir)


def Main():
    util.default_email_recipient = "johannes.riedl@uni-tuebingen.de"
    if len(sys.argv) != 2:
         util.SendEmail("Create Refterm File (Kickoff Failure)",
                        "This script must be called with one argument,\n"
                        + "the default email recipient\n", priority=1);
         sys.exit(-1)
    util.default_email_recipient = sys.argv[1]
    # Download needed differential files
    config = util.LoadConfigFile()
    log_file_name = log_file_name = util.MakeLogFileName(sys.argv[0], util.GetLogDirectory())
    changelist_url = config.get("Unpaywall", "changelist_url")
    api_key = config.get("Unpaywall", "api_key")
    oadoi_download_directory = config.get("LocalConfig", "download_dir")
    oadoi_imported_directory = oadoi_download_directory + "/imported/"
    StartMongoDB()
    json_update_objects = GetChangelists(changelist_url, api_key)
    remote_update_files = GetRemoteUpdateFiles(json_update_objects)
    local_update_files = GetLocalUpdateFiles(config, oadoi_download_directory)
    download_lists = GetAllFilesStartingAtFirstMissingLocal(remote_update_files, local_update_files)
    DownloadUpdateFiles(download_lists['download'], json_update_objects, api_key, oadoi_download_directory)

    # Update the Database
    ImportOADOIsToMongo(GetImportFiles(config, oadoi_download_directory, oadoi_imported_directory), oadoi_download_directory, log_file_name)

    # Generate the files to be used by the pipeline
    share_directory = config.get("LocalConfig", "share_directory")
    ixtheo_dois_file = config.get("LocalConfig", "ixtheo_dois_file")
    ixtheo_urls_file = config.get("LocalConfig", "ixtheo_urls_file")
    ExtractOADOIURLs(share_directory, ixtheo_dois_file, ixtheo_urls_file, log_file_name)
    ShareOADOIURLs(share_directory, ixtheo_urls_file)
    krimdok_dois_file = config.get("LocalConfig", "krimdok_dois_file")
    krimdok_urls_file = config.get("LocalConfig", "krimdok_urls_file")
    ExtractOADOIURLs(share_directory, krimdok_dois_file, krimdok_urls_file, log_file_name)
    ShareOADOIURLs(share_directory, krimdok_urls_file)
    DumpMongoDB(config, log_file_name)
    StopMongoDB()
    util.SendEmail("Update OADOI Data",
                   "Successfully created \"" + ixtheo_urls_file + "\" and \""  + krimdok_urls_file +
                   "\" in " + share_directory, priority=5)


try:
    Main()
except Exception as e:
    error_msg = "An unexpected error occured: " + str(e) + "\n\n" + traceback.format_exc(20)
    util.SendEmail("Update OADOI Data", error_msg, priority=1)
    sys.stderr.write(error_msg)
