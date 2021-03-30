#!/bin/python3
# -*- coding: utf-8 -*-
import github_api_util
import traceback
import util
import zjs_create_issues
import zjs_tag_from_datenprobleme
import zjs_tag_zotaut
import zjs_tag_relbib


def Main():
    github_api_util.ExportPersonalAuthenticationToken()
    zjs_create_issues.CreateNewZoteroJournalStatusIssues()
    zjs_tag_from_datenprobleme.TagZoteroJournalStatusFromDatenProbleme()
    zjs_tag_zotaut.TagZoteroJournalDeliveryStatusFromZeder()
    zjs_tag_relbib.TagRelbibJournalsFromZoteroHarvesterConf()
    util.SendEmail("ZJS Update Journal Status", "Successfully updated zotero-journal-status")


try:
    Main()
except Exception as e:
    util.SendEmail("BSZ File Update", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
