#!/bin/python3
# -*- coding: utf-8 -*-
import github_api_util
import github_ubtue_util
import zotero_harvester_util

ZOTERO_JOURNAL_STATUS_REPO = 'zotero-journal-status'

def GetSSGNStatusFromConfig(config, issn):
    zotero_group = "zotero_group"
    for section in config.sections():
       if config.has_option(section, zotero_group) and config.get(section, zotero_group) in ["IxTheo"]:
           if config.has_option(section, "online_issn") and config.get(section, "online_issn") == issn  \
               or config.has_option(section, "print_issn") and config.get(section, "print_issn") == issn:
                   if not config.has_option(section, "ssgn"):
                       return False
                   return config.get(section, "ssgn")
           


def TagRelbibJournalsFromZoteroHarvesterConf():
    config = zotero_harvester_util.GetZoteroConfiguration()
    zotero_journal_status_issues = github_api_util.GetAllIssuesForUBTueRepository(ZOTERO_JOURNAL_STATUS_REPO)
    for issue in zotero_journal_status_issues:
        issn_matcher = github_api_util.GetISSNMatcher()
        issns = issn_matcher.findall(issue['title'])
        if len(issns):
            for issn in issns:
                ssgn = GetSSGNStatusFromConfig(config, issn)
                has_relbib_ssgn = False
                if ssgn == "FG_0":
                    new_labels = github_ubtue_util.AdjustZoteroStatusLabels(
                                     issue, [ github_ubtue_util.RELBIB_LABEL ],
                                     [github_ubtue_util.IXTHEO_LABEL])
                    has_relbib_ssgn = True
                    break;
                if ssgn == "FG_0/1":
                    new_labels = github_ubtue_util.AdjustZoteroStatusLabels(
                                     issue, [ github_ubtue_util.RELBIB_LABEL ],
                                     [])
                    has_relbib_ssgn = True
                    break;
            if has_relbib_ssgn and not github_ubtue_util.LabelsAreIdentical(issue, new_labels):
               issue_number = str(issue['number'])
               data = { "labels" : new_labels }
               github_api_util.UpdateIssueInRepository('zotero-journal-status', issue_number, data)


def Main():
    TagRelbibJournalsFromZoteroHarvesterConf()

if __name__ == "__main__":
    Main()

