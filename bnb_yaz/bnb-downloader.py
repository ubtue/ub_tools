#!/usr/bin/python2
# -*- coding: utf-8 -*-
#
# A tool for the automation of MARC downloads from the BNB.

import os
import pexpect
import pipes
import process_util
import re
import time
import traceback
import urllib
import util


def GetNewBNBNumbers(list_no):
    pdf_filename = str(list_no) + ".pdf"
    headers = urllib.urlretrieve("https://www.bl.uk/bibliographic/bnbnewpdfs/bnblist%d.pdf"
                                 % list_no, pdf_filename)[1]
    if headers["Content-type"] != "application/pdf":
        util.Remove(pdf_filename)
        return headers["Content-type"]
    pipeline = pipes.Template()
    pipeline.prepend("pdftotext " + pdf_filename + " -", ".-")
    pipeline.append("grep GBB", "--")
    pipeline.append("cut -d\\  -f3", "--")
    pipeline.append("sort > $OUT", "-f")
    numbers_filename = str(list_no) + ".numbers"
    numbers = pipeline.open(numbers_filename, "r")
    list = numbers.read().splitlines()
    numbers.close()
    util.Remove(pdf_filename)
    return list


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
# @note A return code of None indicates w/ a high probablity that a document of "list_no" does not exist on the BNB web server
def RetryGetNewBNBNumbers(list_no):
    NO_OF_ATTEMPTS = 4
    sleep_interval = 10 # seconds
    for attempt in xrange(1, NO_OF_ATTEMPTS):
        print "Attempt #" + str(attempt)
        retval = GetNewBNBNumbers(list_no)
        if type(retval) == list:
            return retval
        else:
            time.sleep(sleep_interval)
            sleep_interval *= 2 # Exponential backoff
    return None


LAST_LIST_NUMBER_FILE = "/usr/local/var/lib/tuelib/cronjobs/bnb-downloader.last_list_number"


def LoadStartListNumber():
    if not os.path.isfile(LAST_LIST_NUMBER_FILE) or not os.access(LAST_LIST_NUMBER_FILE, os.R_OK):
        util.Error(LAST_LIST_NUMBER_FILE " not found.  You must initialise this file.\n"
                   + "It might help to have a look at "
                   + "https://www.bl.uk/bibliographic/natbibweekly.html?_ga=2.146847615.999275275.1565779921-903569786.1548059398")
    with open(LAST_LIST_NUMBER_FILE, "r") as f:
        return int(f.read())


def StoreStartListNumber(last_list_number):
    with open(LAST_LIST_NUMBER_FILE, "w") as f:
        f.write(last_list_number)


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
        yaz_client.expect("Number of hits: (\\d+), setno", timeout=600)
        count_search = re.search("Number of hits: (\\d+), setno", yaz_client.after)
        if count_search:
            download_count += int(count_search.group(1))
        else:
            util.Error('regular expression did not match "' + yaz_client.after + '"!')
        yaz_client.sendline("show all")
        yaz_client.expect("\r\n")
    return download_count

    
def DownloadRecords(yaz_client, output_filename, year, ranges):
    util.Info("year=" + str(year) + ", start_number=" + str(start_number) + ", max_number=" + str(max_number))
    yaz_client.sendline("format marc21")
    yaz_client.expect("\r\n")
    yaz_client.sendline("set_marcdump " + output_filename)
    yaz_client.expect("\r\n")
    return DownloadRecordsRange(yaz_client, ranges)
    
    
def Main():
    OUTPUT_FILENAME = "bnb-" + datetime.datetime.now().strftime("%y%m%d") + ".mrc"
    try:
        os.remove(OUTPUT_FILENAME)
    except:
        pass
    list_number = LoadStartListNumber()
    yaz_client = ConnectToYAZServer()
    total_count = 0
    while True:
        ranges = RetryGetNewBNBNumbers(list_no)
        if ranges is None:
            break
        DownloadRecords(yaz_client, OUTPUT_FILENAME, CoalesceNumbers(ranges))
        ++list_no
    StoreStartListNumber(list_number)
    util.Info("Downloaded " + str(total_count) + " new record(s).")

    
try:
    Main()
except Exception as e:
    util.SendEmail("BNB Downloader", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
