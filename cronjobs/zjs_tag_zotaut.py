#!/bin/python3
# -*- coding: utf-8 -*-
import json
import github_api_util
import github_ubtue_util

ZEDER_URL_IXTHEO = 'http://www-ub.ub.uni-tuebingen.de/zeder/cgi-bin/zeder.cgi?action=get&Dimension=wert&Instanz=ixtheo&Bearbeiter='
ZEDER_URL_KRIMDOK = 'http://www-ub.ub.uni-tuebingen.de/zeder/cgi-bin/zeder.cgi?action=get&Dimension=wert&Instanz=krim&Bearbeiter='

def GetDataFromZeder(zeder_url):
    response = urllib.urlopen(zeder_url)
    jdata = json.load(response)
    return jdata


def HasZotAut(item):
    print(item.get('prodf'))
    return item.get('prodf') == "8"


def GetZederZotAutStatusForISSN(issn, zeder_instances):
    for zeder_instance in zeder_instances:
        for item in zeder_instance['daten']:
             if 'essn' in item and item['essn'] == issn:
                 if HasZotAut(item):
                     print("Matched ESSN: \"" + item.get('tit') + "\" (" + issn + ") [" + (item.get('prodf') if item.get('prodf') is not None else "UNKNOWN") + "]")
             elif 'issn' in item and item['issn'] == issn:
                 if  HasZotAut(item):
                     print("Matched ISSN: \"" + item.get('tit') + "\" (" + issn + ") ["  + item.get('prodf') if item.get('prodf') is not None else "UNKNOWN" + "]")
             #else:
             #    print ("No match for ISSN: " + issn)


def TagZoteroJournalStatusZotAutFromZeder():
    #ixtheo_zeder = GetDataFromZeder(ZEDER_URL_IXTHEO)
    with open("/usr/local/tmp/zjs/zeder_ixtheo.json") as zeder_ixtheo_file:
        ixtheo_zeder = json.load(zeder_ixtheo_file)
    with open("/usr/local/tmp/zjs/zeder_krim.json") as zeder_krim_file:
        krimdok_zeder = json.load(zeder_krim_file)
    #krimdok_zeder = GetDataFromZeder(ZEDER_URL_KRIMDOK)
    #zotero_journal_status_issues = github_api_util.GetAllIssuesForUBTueRepository("zotero-journal-status")
    with open("/usr/local/tmp/zjs/zotero_journal_status_all_issues") as zotero_journal_status_file:
         zotero_journal_status_issues = json.load(zotero_journal_status_file)

    for issue in zotero_journal_status_issues:
        issn_matcher = github_api_util.GetISSNMatcher()
        issns = issn_matcher.findall(issue['title'])
        if len(issns):
            for issn in issns:
                zotaut_status =  GetZederZotAutStatusForISSN(issn, [ixtheo_zeder, krimdok_zeder])



def Main():
    TagZoteroJournalStatusZotAutFromZeder()


if __name__ == "__main__":
    Main()



