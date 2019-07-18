#!/usr/bin/python2
# -*- coding: utf-8 -*-
#
# A tool for the automation of MARC downloads from the BNB.

import datetime
import os
import pexpect
import process_util
import re
import traceback
import util


# See https://www.bl.uk/bibliographic/bnbstruct.html for a chance to understand the following code.
def GenerateBNBNumberPrefix(year):
    return "GB" + [ 'A', 'B', 'C', 'D'][((year - 2000) // 10) - 1] + str(year % 10)


MAX_ANNUAL_NUMBER = 359999 # See https://www.bl.uk/bibliographic/bnbstruct.html to understand this.


# See https://www.bl.uk/bibliographic/bnbstruct.html for a chance to understand the following code.
def GenerateBNBNumberSuffix(n):
    if n < 100000:
        return str(n).zfill(5)
    elif n > MAX_ANNUAL_NUMBER:
        util.Error("can't convert numbers larger than 359,999!")
    else:
        return chr(n // 10000 - 10 + ord('A')) + str(n % 10000).zfill(4)


LAST_MAX_NUMBER_FILE = "/usr/local/var/lib/tuelib/cronjobs/bnb-downloader.last_max_number"


def LoadStartBNBNumber():
    if not os.path.isfile(LAST_MAX_NUMBER_FILE) or not os.access(LAST_MAX_NUMBER_FILE, os.R_OK):
        current_year = datetime.date.today().year
        return GenerateBNBNumberPrefix(current_year) + GenerateBNBNumberSuffix(1)
    with open(LAST_MAX_NUMBER_FILE, "r") as f:
        return f.read()


def StoreStartBNBNumber(last_bnb_number):
    with open(LAST_MAX_NUMBER_FILE, "w") as f:
        f.write(last_bnb_number)


ASCII_CODE_FOR_CAPITAL_A = 65


# See https://www.bl.uk/bibliographic/bnbstruct.html for a chance to understand the following code.
def ExtractBNBNumberSuffixAsInt(bnb_number):
    if type(bnb_number) is not str or len(bnb_number) != 9:
         util.Error("not a BNB number: '" + str(bnb_number) + "'!")
    as_int = int(bnb_number[5:])
    leading_char = bnb_number[4:5]
    if leading_char >= '0' and leading_char <= '9':
        return as_int + 10000 * int(leading_char)
    return as_int + (ord(leading_char) + 10 - ASCII_CODE_FOR_CAPITAL_A) * 10000


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


def BNBNumberIsPresent(yaz_client, bnb_number):
    yaz_client.sendline('find @attr 1=48 "' + bnb_number + '"')
    index = yaz_client.expect(["Number of hits: 1,", "Number of hits: 0,"])
    if index == 0:
        return True
    if index == 1:
        return False
    util.Error("Unexpected server response while looking for BNB number '" + bnb_number + "': " + yaz_client.after)


# @brief Scans the range [lower, MAX_ANNUAL_NUMBER].
# @return The largest BNB number for the current year.
def FindMaxBNBNumber(yaz_client, year, lower):
    prefix = GenerateBNBNumberPrefix(year)
    upper = MAX_ANNUAL_NUMBER
    while lower < upper:
        middle = (lower + upper) // 2
        if middle == lower:
            break
        if BNBNumberIsPresent(yaz_client, prefix + GenerateBNBNumberSuffix(middle)):
            lower = middle
        else:
            upper = middle
    upper_bnb_number = prefix + GenerateBNBNumberSuffix(upper)
    lower_bnb_number = prefix + GenerateBNBNumberSuffix(lower)
    if BNBNumberIsPresent(yaz_client, upper_bnb_number):
        return upper_bnb_number
    elif BNBNumberIsPresent(yaz_client, lower_bnb_number):
        return lower_bnb_number
    util.Error("FindMaxBNBNumber: failed to find the maximum BNB number for the current year!")


# @return a list that contains the wildcards including the requested range and specifying 1000 ID's each.
def EnumerateBNBRange(start_bbn_number, end_bbn_number):
    start_number = (ExtractBNBNumberSuffixAsInt(start_bbn_number) // 1000) * 1000
    end_number   = (ExtractBNBNumberSuffixAsInt(end_bbn_number) // 1000) * 1000
    wildcards = []
    prefix = start_bbn_number[0:4]
    for suffix in range(start_number, end_number + 1, 1000):
        wildcards.append(prefix + GenerateBNBNumberSuffix(suffix)[0:2] + "*")
    return wildcards
    
    
# @return The number of downloaded records.    
def DownloadRecordsRange(yaz_client, prefix, start_number, end_number):
    download_count = 0
    for range in EnumerateBNBRange(prefix + GenerateBNBNumberSuffix(start_number), prefix + GenerateBNBNumberSuffix(end_number)):
        yaz_client.sendline("find @and @attr 1=48 " + range
                            + " @attr 1=13 @or @or @or @or @or @or @or @or @or 20* 21* 22* 23* 24* 25* 26* 27* 28* 29*")
        yaz_client.expect("Number of hits: (\\d+), setno", timeout=300)
        count_search = re.search("Number of hits: (\\d+), setno", yaz_client.after)
        if count_search:
            download_count += int(count_search.group(1))
        else:
            util.Error('regular expression did not match "' + yaz_client.after + '"!')
        yaz_client.sendline("show all")
        yaz_client.expect("\r\n")
    return download_count

    
def DownloadRecords(yaz_client, output_filename, year, start_number, max_number):
    yaz_client.sendline("format marc21")
    yaz_client.expect("\r\n")
    yaz_client.sendline("set_marcdump " + output_filename)
    yaz_client.expect("\r\n")
    prefix = start_number[0:4]
    count = 0
    upper_bound = ExtractBNBNumberSuffixAsInt(max_number)
    for i in range(ExtractBNBNumberSuffixAsInt(start_number), upper_bound + 1, 1000):
        count += DownloadRecordsRange(yaz_client, prefix, i, min(i + 1000 - 1, upper_bound)) 


def FilterBNBNumbers(ranges, marc_filename):
    TEMP_FILENAME = "temp." + marc_filename
    args = ["|".join([range[0] + '-' + range[1] for range in ranges]), "015a", marc_filename, TEMP_FILENAME]
    if process_util.Exec("/usr/local/bin/marc_range_filter", args) != 0:
        util.Error("marc_range_filter " + str(args) + " failed!")
    os.rename(TEMP_FILENAME, marc_filename)
    
    
def Main():
    OUTPUT_FILENAME = "bnb-" + datetime.datetime.now().strftime("%y%m%d") + ".mrc"
    try:
        os.remove(OUTPUT_FILENAME)
    except:
        pass
    START_NUMBER = LoadStartBNBNumber()
    yaz_client = ConnectToYAZServer()
    CURRENT_YEAR = datetime.date.today().year
    ranges = []
    if START_NUMBER[0:4] == GenerateBNBNumberPrefix(CURRENT_YEAR):
        # We handle the current year further down.
        pass
    elif START_NUMBER[0:4] == GenerateBNBNumberPrefix(CURRENT_YEAR - 1):
        max_bnb_number_for_previous_year = FindMaxBNBNumber(yaz_client, CURRENT_YEAR - 1, ExtractBNBNumberSuffixAsInt(START_NUMBER))
        DownloadRecords(yaz_client, OUTPUT_FILENAME, year - 1, START_NUMBER, max_bnb_number_for_previous_year)
        ranges.append((START_NUMBER, max_bnb_number_for_previous_year))
        START_NUMBER = GenerateBNBNumberPrefix(CURRENT_YEAR) + GenerateBNBNumberSuffix(1)
    else:
        util.Error("unexpected: start number '" + START_NUMBER + "' has a prefix matching neither this year nor last year!")

    max_bnb_number_for_current_year = FindMaxBNBNumber(yaz_client, CURRENT_YEAR, ExtractBNBNumberSuffixAsInt(START_NUMBER))
    DownloadRecords(yaz_client, OUTPUT_FILENAME, CURRENT_YEAR, START_NUMBER, max_bnb_number_for_current_year)
    ranges.append((START_NUMBER, max_bnb_number_for_current_year))
    FilterBNBNumbers(ranges, OUTPUT_FILENAME)
    StoreStartBNBNumber(max_bnb_number_for_current_year)


try:
    Main()
except Exception as e:
    util.SendEmail("BNB Downloader", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
