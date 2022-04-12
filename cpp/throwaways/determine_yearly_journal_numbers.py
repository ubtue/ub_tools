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


IXTHEO_ZEDER_URL = 'http://www-ub.ub.uni-tuebingen.de/zeder/cgi-bin/zeder.cgi?action=get&Dimension=wert&Instanz=ixtheo&Bearbeiter='
KRIMDOK_ZEDER_URL = 'http://www-ub.ub.uni-tuebingen.de/zeder/cgi-bin/zeder.cgi?action=get&Dimension=wert&Instanz=krim&Bearbeiter='
ZEDER_URL = ''
IXTHEO_SOLR_BASE_URL = 'http://ptah.ub.uni-tuebingen.de:8983/solr/biblio/select?'
KRIMDOK_SOLR_BASE_URL = 'http://sobek.ub.uni-tuebingen.de:8983/solr/biblio/select?'
SOLR_BASE_URL = ''
KAT_NUM_TO_EVALUATE = ''
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
        #if c.get('kat') != "5":
        if c.get('kat') != KAT_NUM_TO_EVALUATE:
            continue
        journal_ppns = []
        pppn = c['pppn'] if 'pppn' in c else None
        if pppn is not None and pppn != "NV" and pppn != "":
           journal_ppns.append(pppn.strip(' ?'))
        eppn = c['eppn'] if 'eppn' in c else None
        if eppn is not None and eppn != "NV" and eppn != "":
           journal_ppns.append(eppn.strip(' ?'))
        if journal_ppns:
           id_to_journals_ppns[row_id] = journal_ppns
    return id_to_journals_ppns


def AssembleQuery(ppns, year):
    return SOLR_BASE_URL + 'fq=publishDate%3A%5B' \
           + str(year) + '%20TO%20'  + str(year) + '%5D&q=' + \
           'superior_ppn%3A' + \
           ('%20OR%20superior_ppn%3A'.join(ppns) if len(ppns) > 1 else str(ppns[0]))


def GetArticleCount(ppns):
    current_year = date.today().year
    count_dict = {}
    count_total = 0
    for year in range(current_year - 4, current_year + 1):
        query=AssembleQuery(ppns, year)
   #     print("XXXX: " + query)
        request = urllib.request.Request(query, headers=headers)
        response = urllib.request.urlopen(request).read()
        solrdata = json.loads(response.decode('utf-8'))
        count_in_year = solrdata['response']['numFound']
        count_total += count_in_year
        count_dict[str(year)] = count_in_year
    count_dict['all'] = count_total
    return count_dict


def Usage():
    print("Usage " + sys.argv[0] + " fid_system, where fid_system is ixtheo|krimdok")
    exit(-1)


def Main():
   if len(sys.argv) != 2:
        Usage()
   fid_system = sys.argv[1]
   global ZEDER_URL, SOLR_BASE_URL, KAT_NUM_TO_EVALUATE
   if fid_system != "ixtheo" and fid_system != "krimdok":
        Usage()
   if fid_system == "ixtheo":
        ZEDER_URL = IXTHEO_ZEDER_URL
        SOLR_BASE_URL = IXTHEO_SOLR_BASE_URL
        KAT_NUM_TO_EVALUATE = "5"
   else:
       ZEDER_URL = KRIMDOK_ZEDER_URL
       SOLR_BASE_URL = KRIMDOK_SOLR_BASE_URL
       KAT_NUM_TO_EVALUATE = "10"
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
