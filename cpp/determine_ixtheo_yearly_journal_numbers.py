#!/bin/python3
# -*- coding: utf-8 -*-

from subprocess import call
from operator import itemgetter
from datetime import date
import glob
import json
import os
import shutil
import sys
import traceback
import urllib.request


ZEDER_URL = 'http://www-ub.ub.uni-tuebingen.de/zeder/cgi-bin/zeder.cgi?action=get&Dimension=wert&Instanz=ixtheo&Bearbeiter='
IXTHEO_SOLR_BASE_URL = 'http://ptah.ub.uni-tuebingen.de:8983/solr/biblio/select?'
headers = {"Content-type": "application/json", "Accept": "application/json"}

def GetDataFromZeder():
    request = urllib.request.Request(ZEDER_URL, headers=headers)
    response = urllib.request.urlopen(request).read()
    jdata = json.loads(response.decode('utf-8'))
    #response = open("/tmp/zeder_220404.json", 'r')
    #jdata = json.load(response)
    return jdata


def ExtractJournalPPNs(jdata):
    id_to_journals_ppns = {}
    for c in jdata['daten']:
        row_id = c['DT_RowId'] if 'DT_RowId' in c else None
        if row_id is None:
            print("Fatal: No row row is for " + json.dumps(c))
            exit(1)
        # Only extract currently evaluated journals
        if c.get('kat') != "5":
            continue
        journal_ppns = []
        pppn = c['pppn'] if 'pppn' in c else None
        if pppn is not None and pppn != "NV" and pppn != "":
           journal_ppns.append(pppn)
        eppn = c['eppn'] if 'eppn' in c else None
        if eppn is not None and eppn != "NV" and eppn != "":
           journal_ppns.append(eppn)
        if journal_ppns:
           id_to_journals_ppns[row_id] = journal_ppns
    return id_to_journals_ppns


def AssembleQuery(ppns, year):
    return IXTHEO_SOLR_BASE_URL + 'fq=publishDate%3A%5B' \
           + str(year) + '%20TO%20'  + str(year) + '%5D&q=' + \
           'superior_ppn%3A' + \
           ('%20OR%20superior_ppn%3A'.join(ppns) if len(ppns) > 1 else str(ppns[0]))


def GetArticleCount(ppns):
    current_year = date.today().year
    count_dict = {}
    count_total = 0
    for year in range(current_year - 4, current_year + 1):
        query=AssembleQuery(ppns, year)
        request = urllib.request.Request(query, headers=headers)
        response = urllib.request.urlopen(request).read()
        solrdata = json.loads(response.decode('utf-8'))
        count_in_year = solrdata['response']['numFound']
        count_total += count_in_year
        count_dict[str(year)] = count_in_year
    count_dict['all'] = count_total
    return count_dict



def Main():
   jdata = GetDataFromZeder()
   id_to_journals_ppns = ExtractJournalPPNs(jdata)
   all_journals_count_dict = {}
   for row_id, ppns in id_to_journals_ppns.items():
     print (str(row_id), end=":\t")
     count_dict =  GetArticleCount(ppns)
     for year, count in count_dict.items():
         print(str(year) + " : " +  str(count), end = '\t|  ')
         all_journals_count_dict[year] =  all_journals_count_dict[year] + count \
                                          if year in all_journals_count_dict else count
     print('\n')
#     if row_id > 50:
#         break
   print("-------------------------------------------------------------------------------", end='')
   print("----------------------------")
   print("SUM:", end='\t')
   for year,count in all_journals_count_dict.items():
       print(str(year) + " : " +  str(count), end = '\t|  ')
   print('\n')


try:
    Main()
except Exception as e:
    error_msg = "An unexpected error occured: " + str(e) + "\n\n" + traceback.format_exc(20)
    print(error_msg)
