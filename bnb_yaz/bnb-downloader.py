#!/usr/bin/python2
# -*- coding: utf-8 -*-
#
# A tool for the automation of MARC downloads from the BNB.

import datetime
import os
import pexpect
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


# @return The number of downloaded records.    
def DownloadRecordsRange(yaz_client, prefix, start_number, end_number):
    print "in DownloadRecordsRange: prefix=", prefix, ", start_number=", start_number, ", end_number=", end_number
    return 0


def DownloadRecords(yaz_client, year, start_number, max_number):
    if datetime.date.today().year == year:
        week_of_the_year = datetime.date.today().isocalendar()[1]
        upper_bound = int(week_of_the_year * MAX_ANNUAL_NUMBER / 52.0)
    else:
        upper_bound = MAX_ANNUAL_NUMBER
    upper_bound = min(upper_bound, ExtractBNBNumberSuffixAsInt(max_number))
    output_filename = "bnb-" + datetime.datetime.now().strftime("%y%m%d") + ".mrc"
    yaz_client.sendline("format marc21")
    yaz_client.expect("\r\n")
    yaz_client.sendline("set_marcdump " + output_filename)
    yaz_client.expect("\r\n")
    prefix = start_number[0:4]
    count = 0
    for i in range(ExtractBNBNumberSuffixAsInt(start_number), upper_bound + 1, 1000):
        count += DownloadRecordsRange(yaz_client, prefix, i, min(i + 1000 - 1, ExtractBNBNumberSuffixAsInt(max_number))) 


def Main():
    start_number = LoadStartBNBNumber()
    yaz_client = ConnectToYAZServer()
    current_year = datetime.date.today().year
    if start_number[0:4] == GenerateBNBNumberPrefix(current_year):
        pass
    elif start_number[0:4] == GenerateBNBNumberPrefix(current_year - 1):
        max_bnb_number_for_previous_year = FindMaxBNBNumber(yaz_client, current_year - 1, ExtractBNBNumberSuffixAsInt(start_number))
        DownloadRecords(yaz_client, year - 1, start_number, max_bnb_number_for_previous_year)
        start_number = GenerateBNBNumberPrefix(current_year) + GenerateBNBNumberSuffix(1)
    else:
        util.Error("unexpected: start number '" + start_number + "' has a prefix matching neither this year nor last year!")

    max_bnb_number_for_current_year = FindMaxBNBNumber(yaz_client, current_year, ExtractBNBNumberSuffixAsInt(start_number))
    DownloadRecords(yaz_client, current_year, start_number, max_bnb_number_for_current_year)
    StoreBNBNumber(max_bnb_number_for_current_year)


try:
    Main()
except Exception as e:
    util.SendEmail("BNB Downloader", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
