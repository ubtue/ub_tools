#!/bin/python3
# -*- coding: utf-8 -*-

UNTESTED_LABEL = "Untested"
NO_OPEN_ISSUE_LABEL = "No Open Issues"
HAS_ISSUE_LABEL = "Has Issue"
ZOTAUT_LABEL = "ZOTAUT"
READY_FOR_PRODUCTION_LABEL = "Ready for Production"
BSZ_LABEL = "BSZ"
IXTHEO_LABEL = "IxTheo"
KRIMDOK_LABEL = "Krimdok"
RELBIB_LABEL = "RelBib"



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


