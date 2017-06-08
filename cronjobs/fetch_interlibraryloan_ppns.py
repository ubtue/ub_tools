#!/bin/python2
# -*- coding: utf-8 -*-


import datetime
import json
import os
import re
import sys
import time
import traceback
import urllib
import util


GVI_URL          = 'http://gvi.bsz-bw.de/solr/GVI/select?rows=1000000&wt=json&indent=true&fl=id&q=material_media_type:Book+AND+(ill_flag:IllFlag.Loan+OR+ill_flag:IllFlag.Copy+OR+ill_flag:IllFlag.Ecopy)+AND+publish_date:%5B2000+TO+*%5D'
DROP_SIGIL_REGEX = '[(][A-Z]{2}-\d\d\d[)]'
PPN_LIST_FILE    = "gvi_ppn_list-" + datetime.datetime.today().strftime('%y%m%d') + ".txt"


def GetDataFromGVI():
    response = urllib.urlopen(GVI_URL)
    try:
        jdata = json.load(response)
    except ValueError:
        util.Error("GVI did not return valid JSON!")
    return jdata


def ExtractPPNs(jdata):
    drop_pattern = re.compile(DROP_SIGIL_REGEX)
    ppns = []
    for doc in jdata['response']['docs']:
        if doc.get('id') is not None:
            sigil_and_ppn = doc.get('id')
            ppn = drop_pattern.sub("", sigil_and_ppn)
            ppns.append(ppn)
    return ppns


def WriteListFile(ppn_list):
    with open(PPN_LIST_FILE, 'w') as file:
        file.writelines("%s\n" % ppn for ppn in ppn_list)


def Main():
    if len(sys.argv) != 2:
        util.SendEmail(os.path.basename(sys.argv[0]),
                       "This script needs to be called with an email address as the only argument!\n", priority=1)
    jdata = GetDataFromGVI()
    ppn_list = ExtractPPNs(jdata)
    WriteListFile(ppn_list)


try:
    Main()
except Exception as e:
    error_msg = "An unexpected error occured: " + str(e) + "\n\n" + traceback.format_exc(20)
    util.Error(error_msg)
