#!/usr/bin/python2
# -*- coding: utf-8 -*-
#
# A tool for the automation of MARC downloads from the BNB.

import datetime
import os
import pexpect
import traceback
import util


LAST_MAX_NUMBER_FILE = "/usr/local/var/lib/tuelib/cronjobs/bnb-downloader.last_max_number"


def LoadStartNumber():
    if not os.path.isfile(LAST_MAX_NUMBER_FILE) or not os.access(LAST_MAX_NUMBER_FILE, os.R_OK):
        return 1
    with open(LAST_MAX_NUMBER_FILE, "r") as f:
        return int(f.read())


def StoreStartNumber(last_number):
    with open(LAST_MAX_NUMBER_FILE, "w") as f:
        f.write(str(last_number))


# See https://www.bl.uk/bibliographic/bnbstruct.html for a chance to understand the following code.
def GenerateBNBNumberPrefix():
    return "GB" + [ 'A', 'B', 'C', 'D'][((datetime.date.today().year - 2000) // 10) - 1] + str(datetime.date.today().year % 10)


MAX_ANNUAL_NUMBER = 359999 # See https://www.bl.uk/bibliographic/bnbstruct.html to understand this.


# See https://www.bl.uk/bibliographic/bnbstruct.html for a chance to understand the following code.
def GenerateBNBNumberSuffix(n):
    if n < 100000:
        return str(n).zfill(5)
    elif n > MAX_ANNUAL_NUMBER:
        util.Error("can't convert numbers larger than 359,999!")
    else:
        return chr(n // 10000 - 10 + ord('A')) + str(n % 10000).zfill(4)


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
def FindMaxBNBNumber(yaz_client, lower):
    prefix = GenerateBNBNumberPrefix()
    upper = MAX_ANNUAL_NUMBER
    while lower < upper:
        middle = (lower + upper) // 2
        if middle == lower:
            break
        if BNBNumberIsPresent(yaz_client, prefix + GenerateBNBNumberSuffix(middle)):
            lower = middle
        else:
            upper = middle - 1
    max_bnb_number = prefix + GenerateBNBNumberSuffix(lower)
    if BNBNumberIsPresent(yaz_client, max_bnb_number):
        return max_bnb_number
    util.Error("FindMaxBNBNumber: failed to find the maximum BNB number for the current year!")


def Main():
    yaz_client = ConnectToYAZServer()
    print FindMaxBNBNumber(yaz_client)


try:
    Main()
except Exception as e:
    util.SendEmail("BNB Downloader", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
