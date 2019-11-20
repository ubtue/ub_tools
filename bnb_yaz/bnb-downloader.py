#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# A tool for the automation of MARC downloads from the BNB.

import datetime
import os
import pexpect
import pipes
import process_util
import re
import sys
import time
import traceback
import urllib.request
import util
import xml.etree.ElementTree as ElementTree
import zipfile


def GetNewBNBNumbers(list_no):
    zipped_rdf_filename = "bnbrdf_N" + str(list_no) + ".zip"
    try:
        headers = urllib.request.urlretrieve("https://www.bl.uk/bibliographic/bnbrdf/bnbrdf_N%d.zip" % list_no, zipped_rdf_filename)[1]
    except urllib.error.URLError as url_error:
        return []
    except urllib.error.HTTPError as http_error:
        print("HTTP error reason: " + http_error.reason)
        print("HTTP headers: " + str(http_error.headers))
        sys.exit(-2)
        
    if headers["Content-type"] != "application/zip":
        util.Remove(zipped_rdf_filename)
        return headers["Content-type"]

    print("Downloaded " + zipped_rdf_filename)
    with zipfile.ZipFile(zipped_rdf_filename, "r") as zip_file:
        zip_file.extractall()
    util.Remove(zipped_rdf_filename)
    rdf_filename = "bnbrdf_N" + str(list_no) + ".rdf"

    numbers = []
    print("About to parse " + rdf_filename)
    tree = ElementTree.parse(rdf_filename)
    for child in tree.iter('{http://purl.org/dc/terms/}identifier'):
        if child.text[0:2] == "GB":
            numbers.append(child.text)
    util.Remove(rdf_filename)
    return numbers


#  Attempts to group sequential numbers from 0 to 9 with a ? wildcard.
def CoalesceNumbers(individual_numbers):
    compressed_list = []
    subsequence = []
    prefix = ""
    for number in individual_numbers:
        current_prefix = number[0:8]
        if current_prefix != prefix:
            if len(subsequence) == 10:
                compressed_list.append(prefix + "?")
            else:
                compressed_list.extend(subsequence)
            subsequence = []
            prefix = current_prefix
        subsequence.append(number)
    compressed_list.extend(subsequence)
    return compressed_list


# @return Either a list of BNB numbers or None
# @note A return code of None indicates w/ a high probablity that a document for "list_no" does not exist on the BNB web server
def RetryGetNewBNBNumbers(list_no):
    util.Info("Downloading BBN numbers for list #" + str(list_no))
    MAX_NO_OF_ATTEMPTS = 4
    sleep_interval = 10 # initial sleep interval after a failed attempt in seconds
    for attempt in range(1, MAX_NO_OF_ATTEMPTS):
        print("Attempt #" + str(attempt))
        retval = GetNewBNBNumbers(list_no)
        if type(retval) == list:
            print("Downloaded and extracted " + str(len(retval)) + " BNB numbers.")
            return None if len(list) == 0 else list
        else:
            print("Content-type of downloaded document was " + retval)
            time.sleep(sleep_interval)
            sleep_interval *= 2 # Exponential backoff
    return None


LAST_LIST_NUMBER_FILE = "/usr/local/var/lib/tuelib/cronjobs/bnb-downloader.last_list_number"


def LoadStartListNumber():
    if not os.path.isfile(LAST_LIST_NUMBER_FILE) or not os.access(LAST_LIST_NUMBER_FILE, os.R_OK):
        util.Error(LAST_LIST_NUMBER_FILE + " not found.  You must initialise this file.\n"
                   + "It might help to have a look at "
                   + "https://www.bl.uk/bibliographic/natbibweekly.html?_ga=2.146847615.999275275.1565779921-903569786.1548059398")
    with open(LAST_LIST_NUMBER_FILE, "r") as f:
        return int(f.read())


def StoreStartListNumber(last_list_number):
    with open(LAST_LIST_NUMBER_FILE, "w") as f:
        f.write(str(last_list_number))


def ConnectToYAZServer():
    with open("/usr/local/var/lib/tuelib/bnb_username_password.conf", "r") as f:
        username_password = f.read()
    yaz_client = pexpect.spawn("yaz-client")
    yaz_client.sendline("auth " + username_password)
    yaz_client.expect("Authentication set to Open.*")
    yaz_client.sendline("open z3950cat.bl.uk:9909")
    yaz_client.expect(".*Connection accepted.*")
    yaz_client.sendline("base BNB03U")
    return yaz_client


# @return The number of downloaded records.
def DownloadRecordsRange(yaz_client, ranges):
    download_count = 0
    for range in ranges:
        yaz_client.sendline("find @and @attr 1=48 " + '"' + range + '"'
                            + " @attr 1=13 @or @or @or @or @or @or @or @or @or 20* 21* 22* 23* 24* 25* 26* 27* 28* 29*")
        yaz_client.expect("Number of hits: (\\d+), setno", timeout=1000)
        count_search = re.search(b"Number of hits: (\\d+), setno", yaz_client.after)
        if count_search:
            download_count += int(count_search.group(1))
        else:
            util.Error('regular expression did not match "' + yaz_client.after + '"!')
        yaz_client.sendline("show all")
        yaz_client.expect("\r\n")
    return download_count


def Main():
    OUTPUT_FILENAME = "bnb-" + datetime.datetime.now().strftime("%y%m%d") + ".mrc"
    try:
        os.remove(OUTPUT_FILENAME)
    except:
        pass

    yaz_client = ConnectToYAZServer()
    yaz_client.sendline("format marc21")
    yaz_client.expect("\r\n")
    yaz_client.sendline("set_marcdump " + OUTPUT_FILENAME)
    yaz_client.expect("\r\n")

    list_no = LoadStartListNumber()
    total_count = 0
    while True:
        ranges = RetryGetNewBNBNumbers(list_no)
        if ranges is None:
            break
        count = DownloadRecordsRange(yaz_client, CoalesceNumbers(ranges))
        util.Info("Dowloaded " + str(count) + " records for list #" + str(list_no) + ".")
        total_count += count
        list_no += 1
    StoreStartListNumber(list_no)
    util.Info("Downloaded a total of " + str(total_count) + " new record(s).")


try:
    Main()
except Exception as e:
    util.SendEmail("BNB Downloader", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
