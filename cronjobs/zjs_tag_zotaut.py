#!/bin/python3
# -*- coding: utf-8 -*-
import json
import urllib.request
import github_api_util
import github_ubtue_util


ZEDER_URL_IXTHEO = 'http://www-ub.ub.uni-tuebingen.de/zeder/cgi-bin/zeder.cgi?action=get&Dimension=wert&Instanz=ixtheo&Bearbeiter='
ZEDER_URL_KRIMDOK = 'http://www-ub.ub.uni-tuebingen.de/zeder/cgi-bin/zeder.cgi?action=get&Dimension=wert&Instanz=krim&Bearbeiter='


def GetDataFromZeder(zeder_url):
    response = urllib.request.urlopen(zeder_url)
    jdata = json.load(response)
    return jdata


def HasZotAut(item, zota_number_code):
    return item.get('prodf') == zota_number_code # zota == 8 for IxTheo instance and zota == 6 for KrimDok instance

def HasZotTest(item, zotat_number_code):
    return item.get('prode') == zotat_number_code


def GetZederZotaNumberCode(zeder_instance):
    for key in zeder_instance['meta']:
        if key['Kurz'] == "prodf":
            for option in key['Optionen']:
                if option['wert'] == "zota":
                   return str(option['id'])
    return "Unknown"


def GetZederZotatNumberCode(zeder_instance):
    for key in zeder_instance['meta']:
        if key['Kurz'] == "prode":
            for option in key['Optionen']:
                if option['wert'] == "zotat":
                   return str(option['id'])
    return "Unknown"


def GetZederZotAutStatusForISSN(issn, zeder_instances):
    for zeder_instance in zeder_instances:
        zota_number_code = GetZederZotaNumberCode(zeder_instance) # We have different number codes for zota depending on the instance
        if zota_number_code == "Unknown":
            raise Exception("Could not determine Id for Zeder zota")
        for item in zeder_instance['daten']:
             if 'essn' in item and item['essn'].strip() == issn:
                 if HasZotAut(item, zota_number_code):
                     return True
             elif 'issn' in item and item['issn'].strip() == issn:
                 if  HasZotAut(item, zota_number_code):
                     return True
    return False


def GetZederZotTestStatusForISSN(issn, zeder_instances):
    for zeder_instance in zeder_instances:
        zotat_number_code = GetZederZotatNumberCode(zeder_instance) # We have different number codes for zotat depending on the instance
        if zotat_number_code == "Unknown":
            raise Exception("Could not determine Id for Zeder zotat")
        for item in zeder_instance['daten']:
             if 'essn' in item and item['essn'].strip() == issn:
                 if HasZotTest(item, zotat_number_code):
                     return True
             elif 'issn' in item and item['issn'].strip() == issn:
                 if  HasZotTest(item, zotat_number_code):
                     return True
    return False



def TagZoteroJournalDeliveryStatusFromZeder():
    ixtheo_zeder = GetDataFromZeder(ZEDER_URL_IXTHEO)
    krimdok_zeder = GetDataFromZeder(ZEDER_URL_KRIMDOK)
    zotero_journal_status_issues = github_api_util.GetAllIssuesForUBTueRepository("zotero-journal-status")
    for issue in zotero_journal_status_issues:
        issn_matcher = github_api_util.GetISSNMatcher()
        issns = issn_matcher.findall(issue['title'])
        zotaut_status = False
        zottest_status = False
        if len(issns):
            for issn in issns:
                zotaut_status = GetZederZotAutStatusForISSN(issn, [ixtheo_zeder, krimdok_zeder])
                if zotaut_status:
                    break
                zottest_status = GetZederZotTestStatusForISSN(issn, [ixtheo_zeder, krimdok_zeder])
                if zottest_status:
                    break
        if zotaut_status:
            new_labels = github_ubtue_util.AdjustZoteroStatusLabels(issue, [ github_ubtue_util.ZOTAUT_LABEL],
                             [ github_ubtue_util.READY_FOR_PRODUCTION_LABEL, github_ubtue_util.BSZ_LABEL,
                               github_ubtue_util.UNTESTED_LABEL ])
            if not github_ubtue_util.LabelsAreIdentical(issue, new_labels):
                issue_number = str(issue['number'])
                data = { "labels" : new_labels }
                github_api_util.UpdateIssueInRepository('zotero-journal-status', issue_number, data)
            continue
        if zottest_status:
            new_labels = github_ubtue_util.AdjustZoteroStatusLabels(issue, [ github_ubtue_util.BSZ_LABEL ],
                             [github_ubtue_util.ZOTAUT_LABEL, github_ubtue_util.UNTESTED_LABEL])
            if not github_ubtue_util.LabelsAreIdentical(issue, new_labels):
                issue_number = str(issue['number'])
                data = { "labels" : new_labels }
                github_api_util.UpdateIssueInRepository('zotero-journal-status', issue_number, data)
            continue
        else:
            new_labels = github_ubtue_util.AdjustZoteroStatusLabels(issue, [],
                             [github_ubtue_util.ZOTAUT_LABEL, github_ubtue_util.BSZ_LABEL])
            if not github_ubtue_util.LabelsAreIdentical(issue, new_labels):
                issue_number = str(issue['number'])
                data = { "labels" : new_labels }
                github_api_util.UpdateIssueInRepository('zotero-journal-status', issue_number, data)



def Main():
    TagZoteroJournalDeliveryStatusFromZeder()


if __name__ == "__main__":
    Main()
