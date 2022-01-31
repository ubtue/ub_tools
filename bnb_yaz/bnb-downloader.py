#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# A tool for the automation of MARC downloads from the BNB.

import bsz_util
import datetime
import os
import pexpect
import process_util
import re
import sys
import time
import traceback
import urllib.request
import util
import xml.etree.ElementTree as ElementTree
import zipfile

dryrun = False
dryrun_list_no = 3677
number_of_runs = 2
max_download_lists = 50

def ExtractRelevantIds(rdf_xml_document):
    numbers = []
    tree = ElementTree.parse(rdf_xml_document)
    root = tree.getroot()
    total_numbers = 0
    for child in root:
        total_numbers += 1
        ddc = ""
        for subject in child.iter('{http://purl.org/dc/terms/}subject'):
            for subject_child in subject.iter("{http://www.w3.org/2004/02/skos/core#}notation"):
                ddc = subject_child.text
        if ddc and ddc[0] != '2': # Theology
            continue
        number = ""
        for identifier in child.iter('{http://purl.org/dc/terms/}identifier'):
            if identifier.text[0:2] == "GB":
                number = identifier.text
        if number:
            numbers.append(number)
    return numbers

    
def GetNewBNBNumbers(list_no):
    zipped_rdf_filename = "bnbrdf_n" + str(list_no) + ".zip"
    download_url = \
        "https://www.bl.uk/britishlibrary/~/media/bl/global/services/collection%20metadata/pdfs/bnb%20records%20rdf/" \
        + zipped_rdf_filename
    if not util.WgetFetch(download_url):
        util.Info("Failed to retrieve '" + download_url + "'!")
        return []
    with zipfile.ZipFile(zipped_rdf_filename, "r") as zip_file:
        zip_file.extractall()
    util.Remove(zipped_rdf_filename)
    rdf_filename = "bnbrdf_N" + str(list_no) + ".rdf"
    numbers = ExtractRelevantIds(rdf_filename)
    util.Remove(rdf_filename)
    return numbers


#  Attempts to group sequential numbers from 0 to 9 with a ? wildcard.
def CoalesceNumbers(individual_numbers):
    individual_numbers.sort()
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
    util.Info("Downloading BNB numbers for list #" + str(list_no))
    MAX_NO_OF_ATTEMPTS = 4
    sleep_interval = 10 # initial sleep interval after a failed attempt in seconds
    for attempt in range(1, MAX_NO_OF_ATTEMPTS):
        print("Attempt #" + str(attempt))
        retval = GetNewBNBNumbers(list_no)
        if type(retval) == list:
            print("Downloaded and extracted " + str(len(retval)) + " BNB numbers.")
            return None if len(retval) == 0 else retval
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
    yaz_client.expect("\r\n")
    yaz_client.sendline("marccharset MARC8/UTF8")
    yaz_client.expect("\r\n")
    return yaz_client


# @return The number of downloaded records.
def DownloadRecordsRanges(yaz_client, ranges):
    download_count = 0
    total_number_of_ranges = len(ranges)
    current_range = 0
    for range in ranges:
        current_range += 1
        util.Info("Processing range #" + str(current_range) + " of " + str(total_number_of_ranges) + " ranges.")
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


def SetupWorkDirectory():
    work_directory: str = "/tmp/bnb-downloader.work"
    try:
        os.mkdir(work_directory, mode=0o744)
    except:
        pass
    os.chdir(work_directory)

    
def UploadToBSZFTPServer(remote_folder_path: str, marc_filename: str):
    remote_file_name_tmp: str = marc_filename + ".tmp"

    ftp = bsz_util.GetFTPConnection()
    ftp.changeDirectory(remote_folder_path)
    ftp.uploadFile(marc_filename, remote_file_name_tmp)
    ftp.renameFile(remote_file_name_tmp, marc_filename)


def Main():
    if dryrun != True:
        if len(sys.argv) != 2:
            print("usage: " + sys.argv[0] + " <default email recipient>")
            util.SendEmail("BNB Downloader Invocation Failure",
                            "This script needs to be called with one argument, the email address!\n", priority=1)
            sys.exit(-1)

        util.default_email_recipient = sys.argv[1]

    SetupWorkDirectory()
    OUTPUT_FILENAME_PREFIX: str = "bnb-" + datetime.datetime.now().strftime("%y%m%d") + "-"
    FTP_UPLOAD_DIRECTORY: str = "pub/UBTuebingen_BNB"

    if dryrun == True:
        list_no = dryrun_list_no
    else:
        list_no = LoadStartListNumber()

    loop_counter = 0
    while True:
        loop_counter += 1
        if loop_counter > max_download_lists:
            break
        for run in range(number_of_runs):
            last_run = (run == number_of_runs-1)
            util.Info("Run " + str(run+1) + " out of " + str(number_of_runs) + " last run will be uploaded. Last run: " + str(last_run))
            yaz_client = ConnectToYAZServer()
            yaz_client.sendline("format marc21")
            yaz_client.expect("\r\n")

            total_count = 0
            OUTPUT_FILENAME: str = None

            util.Info("About to process list #" + str(list_no))
            bnb_numbers = RetryGetNewBNBNumbers(list_no)
            if bnb_numbers is None:
                loop_counter = max_download_lists
                break
            util.Info("Retrieved " + str(len(bnb_numbers)) + " BNB numbers for list #" + str(list_no))
            if len(bnb_numbers) == 0:
                list_no += 1
                if dryrun != True:
                    StoreStartListNumber(list_no)
                break

            # Open new MARC dump file for the current list:
            OUTPUT_FILENAME = OUTPUT_FILENAME_PREFIX + str(list_no) + ".mrc"
            util.Remove(OUTPUT_FILENAME)
            yaz_client.sendline("set_marcdump " + OUTPUT_FILENAME)
            yaz_client.expect("\r\n")
        
            ranges = CoalesceNumbers(bnb_numbers)
            util.Info("The BNB numbers were coalesced into " + str(len(ranges)) + " ranges.")

            count: int = DownloadRecordsRanges(yaz_client, ranges)
            util.Info("Downloaded " + str(count) + " records for list #" + str(list_no) + ".")
            yaz_client.sendline("exit")
            yaz_client.expect(pexpect.EOF)
            #yaz_client.close()
            if count > 0 and dryrun != True and last_run == True:
                UploadToBSZFTPServer(FTP_UPLOAD_DIRECTORY, OUTPUT_FILENAME)
            if last_run == True:
                total_count += count
                if dryrun != True:
                    list_no += 1
                    StoreStartListNumber(list_no)
        #end multiple run loop
        if dryrun == True:
            break
    #end while true

    if OUTPUT_FILENAME is not None and dryrun != True:
        util.Remove(OUTPUT_FILENAME)
    util.Info("Downloaded a total of " + str(total_count) + " new record(s).")
    if dryrun != True:
        if total_count > 0:
            util.SendEmail("BNB Downloader", "Uploaded " + str(total_count) + " records to the BSZ FTP-server.")
        else:
            util.SendEmail("BNB Downloader", "No new records found.")


try:
    Main()
except Exception as e:
    if dryrun != True:
        util.SendEmail("BNB Downloader", "An unexpected error occurred: "
                    + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
    else:
        print(e)
