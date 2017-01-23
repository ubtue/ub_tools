#!/bin/python2
# -*- coding: utf-8 -*-

from subprocess import call
from operator import itemgetter
import datetime
import json
import os
import re
import sys
import time
import traceback
import urllib

reload(sys)
sys.setdefaultencoding('utf-8')

GVI_URL = 'http://gvi.bsz-bw.de/solr/GVI/select?rows=1000000&wt=json&indent=true&fl=id&q=material_media_type:Book+AND+(ill_flag:IllFlag.Loan+OR+ill_flag:IllFlag.Copy+OR+ill_flag:IllFlag.Ecopy)+AND+publish_date:%5B2000+TO+*%5D'
DROP_SIGIL_REGEX = '[(][A-Z]{2}-\d\d\d[)]'
PPN_LIST_FILE = "gvi_ppn_list-" + datetime.datetime.today().strftime('%y%m%d') + ".txt"

def GetDataFromGVI():
    response = urllib.urlopen(GVI_URL)
    jdata = json.load(response)
    return jdata


def ExtractPPNs(jdata):
    drop_pattern = re.compile(DROP_SIGIL_REGEX)
    ppns = []
    for c in jdata['response']['docs']:
        if c.get('id') is not None:
            ppn_full = c.get('id')
            ppn = drop_pattern.sub("", ppn_full)
            ppns.append(ppn)
    return ppns


def WriteListFile(ppn_list):
    file = open(PPN_LIST_FILE, 'w')
    for ppn in ppn_list:
        file.write(ppn + '\n')
    file.close()


def Main():
    jdata = GetDataFromGVI()
    ppn_list = ExtractPPNs(jdata)
    WriteListFile(ppn_list)
   
try:
    Main()
except Exception as e:
    error_msg = "An unexpected error occured: " + str(e) + "\n\n" + traceback.format_exc(20)
    sys.stderr.write(error_msg)
