#!/bin/python3
# -*- coding: utf-8 -*-
import util
import github_api_util
import zjs_create_issues
import zjs_tag_from_datenprobleme


def Main():
    github_api_util.ExportPersonalAuthenticationToken()
    zjs_create_issues.CreateNewZoteroJournalStatusIssues()
    zjs_tag_from_datenprobleme.TagZoteroJournalStatusFromDatenProbleme()
    util.SendEmail("ZJS Update Journal Status", "Successfully updated zotero-journal-status")


try:
    Main()
except Exception as e:
    util.SendEmail("BSZ File Update", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
