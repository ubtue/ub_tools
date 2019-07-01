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


GVI_URL          = 'http://gvi.bsz-bw.de/solr/GVI/select?rows=10000&wt=json&indent=true&fl=id&q=material_media_type:Book+AND+(ill_flag:Loan+OR+ill_flag:Copy+OR+ill_flag:Ecopy)+AND+publish_date:[2000+TO+*]&sort=id+asc&fq=id:\(DE-627\)*'
DROP_SIGIL_REGEX = '[(][A-Z]{2}-\d\d\d[)]'
PPN_LIST_FILE    = "gvi_ppn_list-" + datetime.datetime.today().strftime('%y%m%d') + ".txt"


def GetDataFromGVI(cursor_mark):
    response = urllib.urlopen(GVI_URL + '&cursorMark=' + cursor_mark)
    try:
        jdata = json.load(response)
    except ValueError:
        util.Error("GVI did not return valid JSON!")
    return jdata


def ExtractPPNs(jdata, ppns):
    drop_pattern = re.compile(DROP_SIGIL_REGEX)
    for doc in jdata['response']['docs']:
        if doc.get('id') is not None:
            sigil_and_ppn = doc.get('id')
            ppn = drop_pattern.sub("", sigil_and_ppn)
            ppns.append(ppn)
    return ppns


def WriteListFile(ppn_list):
    with open(PPN_LIST_FILE, 'w') as file:
        file.writelines("%s\n" % ppn for ppn in ppn_list)


def ExtractNextCursorMark(jdata):
    return jdata['nextCursorMark']


def Main():
    if len(sys.argv) != 2:
        util.SendEmail(os.path.basename(sys.argv[0]),
                       "This script needs to be called with an email address as the only argument!\n", priority=1)
    ppns = []
    currentCursorMark = '*'
    while True:
        jdata = GetDataFromGVI(currentCursorMark)
        ppns = ExtractPPNs(jdata, ppns)
        newCursorMark = ExtractNextCursorMark(jdata)
        if currentCursorMark == newCursorMark:
           break
        currentCursorMark = newCursorMark
    WriteListFile(ppns)


try:
    Main()
except Exception as e:
    error_msg = "An unexpected error occured: " + str(e) + "\n\n" + traceback.format_exc(20)
    util.Error(error_msg)
