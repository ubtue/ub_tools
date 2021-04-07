#!/bin/python3
# -*- coding: utf-8 -*-
import json
import os
import stdnum.issn as issn_checker
import urllib.request
import github_api_util
import zotero_harvester_util


ZOTERO_JOURNAL_STATUS_REPO = 'zotero-journal-status'
NO_KNOWN_ISSN = 'No known ISSN'


class EntryPresent(Exception):
    pass


def CreateIssueInZoteroJournalStatus(data):
    if not 'GITHUB_OAUTH_TOKEN' in os.environ:
       raise Exception('GITHUB_OAUTH_TOKEN must be set. Export it from the shell')
    github_oauth_token = os.environ.get('GITHUB_OAUTH_TOKEN')
    jsondata = json.dumps(data)
    bindata = jsondata.encode('utf-8')
    req = urllib.request.Request('https://api.github.com/repos/ubtue/' + ZOTERO_JOURNAL_STATUS_REPO + '/issues')
    req.add_header('Content-Type', 'application/json')
    req.add_header('Authorization', 'token ' + github_oauth_token)
    req.add_header('Content-Length', len(bindata))
    response = urllib.request.urlopen(req, bindata)
    print(response.read().decode('utf-8'))


def CreateNewZoteroJournalStatusIssues():
    journal_types = [ "IxTheo", "KrimDok" ]
    zeder_titles = {}
    zotero_group = "zotero_group"
    config = zotero_harvester_util.GetZoteroConfiguration()
    github_existing_issues = github_api_util.GetAllIssuesForUBTueRepository(ZOTERO_JOURNAL_STATUS_REPO)
    for section in config.sections():
        if config.has_option(section, zotero_group) and config.get(section, zotero_group) in journal_types:
            issn = NO_KNOWN_ISSN
            if config.has_option(section, "online_issn"):
                issn = config.get(section, "online_issn")
            elif config.has_option(section, "print_issn"):
                issn = config.get(section, "print_issn")
            #Filter garbage
            issn = issn if issn_checker.is_valid(issn) else NO_KNOWN_ISSN
            try:
                for issue in github_existing_issues:
                    if issn != NO_KNOWN_ISSN:
                        if issue['title'].find(issn) != -1:
                            raise EntryPresent()
                    else:
                        if issue['title'].find(section) != -1:
                            raise EntryPresent()
            except EntryPresent:
                continue
            zeder_titles.update({ issn + ' | ' + section : config.get(section, zotero_group) })
    for issue_title in zeder_titles:
        data = { "title" : issue_title, "labels" : [ zeder_titles[issue_title], "Untested" ] }
        CreateIssueInZoteroJournalStatus(data)


def Main():
    CreateNewZoteroJournalStatusIssues()


if __name__ == "__main__":
    Main()
