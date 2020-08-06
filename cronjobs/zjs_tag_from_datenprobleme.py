#!/bin/python3
# -*- coding: utf-8 -*-
import urllib.parse
import github_api_util


UNTESTED_LABEL = "Untested"
NO_OPEN_ISSUE_LABEL = "No Open Issues"
HAS_ISSUE_LABEL = "Has Issue"


def ExtractOpenAndClosedWithoutOpenISSNs(issues):
    closed_issues_issns = set()
    open_issues_issns = set()
    issn_matcher = github_api_util.GetISSNMatcher()
    for issue in issues:
        title = issue['title']
        issns = issn_matcher.findall(title)
        for issn in issns:
            if issue['state'] == "open":
                open_issues_issns.add(issn)
            elif issue['state'] == "closed":
                closed_issues_issns.add(issn)
    closed_without_open_issues_issns = closed_issues_issns - open_issues_issns
    return open_issues_issns, closed_without_open_issues_issns


def AdjustZoteroStatusLabels(issue, labels_to_add, labels_to_remove):
    current_labels = issue['labels']
    new_labels = []
    for label in current_labels:
        if not label['name'] in labels_to_remove:
            new_labels.append(label['name'])
    for label_to_add in labels_to_add:
        new_labels.append(label_to_add)
    return new_labels


def LabelsAreIdentical(issue, new_labels):
    current_labels = issue['labels']
    old_labels = []
    for label in current_labels:
        old_labels.append(label['name'])
    return set(old_labels) == set(new_labels)


def UpdateZoteroJournalStatus(issue, labels_to_add, issn):
    if len(labels_to_add) == 0:
        return

    encoded_issn = urllib.parse.quote(issn)
    if HAS_ISSUE_LABEL in labels_to_add:
        new_labels = AdjustZoteroStatusLabels(issue, labels_to_add, [ UNTESTED_LABEL, NO_OPEN_ISSUE_LABEL ]);
        data = { "labels" : new_labels, "body" : "[Open issues](https://github.com/ubtue/DatenProbleme/issues?q=is%3Aissue+is%3Aopen+" \
                                                   + encoded_issn + "+in%3Atitle+) " + \
                                                 "[All Issues](https://github.com/ubtue/DatenProbleme/issues?q=is%3Aissue+" \
                                                   + encoded_issn + "+in%3Atitle+)" }
    elif NO_OPEN_ISSUE_LABEL in labels_to_add:
        new_labels = AdjustZoteroStatusLabels(issue, labels_to_add, [ UNTESTED_LABEL, HAS_ISSUE_LABEL ]);
        data = { "labels" : new_labels, "body" : "[Closed issues](https://github.com/ubtue/DatenProbleme/issues?q=is%3Aissue+is%3Aclosed+" \
                                                   + encoded_issn + "+in%3Atitle+)" }
    else:
        data = { "labels" : new_labels }
    issue_number = str(issue['number'])
    if not LabelsAreIdentical(issue, new_labels):
       github_api_util.UpdateIssueInRepository('zotero-journal-status', issue_number, data)


def TagZoteroJournalStatusFromDatenProbleme():
    datenprobleme_issues = github_api_util.GetAllIssuesForUBTueRepository("Datenprobleme")
    zotero_journal_status_issues = github_api_util.GetAllIssuesForUBTueRepository("zotero-journal-status")
    datenprobleme_open_issns, datenprobleme_closed_without_open_issues_issns = ExtractOpenAndClosedWithoutOpenISSNs(datenprobleme_issues)
    for issue in zotero_journal_status_issues:
        issn_matcher = github_api_util.GetISSNMatcher()
        issns = issn_matcher.findall(issue['title'])
        if len(issns):
            for issn in issns:
                if issn in datenprobleme_open_issns:
                    UpdateZoteroJournalStatus(issue, [ HAS_ISSUE_LABEL ], issn)
                elif issn in datenprobleme_closed_without_open_issues_issns:
                    UpdateZoteroJournalStatus(issue, [ NO_OPEN_ISSUE_LABEL ], issn)


def Main():
    TagZoteroJournalStatusFromDatenProbleme()


if __name__ == "__main__":
    Main()
