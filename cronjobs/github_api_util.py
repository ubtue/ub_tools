# Python 3 module
# -*- coding: utf-8 -*-
import os
import re
import requests
import util


def GetAllIssuesForUBTueRepository(repository):
    results = []
    req = requests.get('https://api.github.com/repos/ubtue/' + repository + '/issues?state=all&per_page=100')
    for result in req.json():
        results.append(result)
    while "next" in req.links:
        next_url =  req.links['next']['url'];
        req = requests.get(next_url)
        for result in req.json():
            results.append(result)
    return results


def UpdateIssueInRepository(repository, issue_number, data):
    if not 'GITHUB_OAUTH_TOKEN' in os.environ:
       raise Exception('GITHUB_OAUTH_TOKEN must be set. Export it from the shell')
    github_oauth_token = os.environ.get('GITHUB_OAUTH_TOKEN')
    headers = { 'Authorization' :  'token ' + github_oauth_token }
    url = 'https://api.github.com/repos/ubtue/' + repository + '/issues/' + issue_number
    req = requests.post(url, json=data, headers=headers)
    req.raise_for_status()
    print(req.text)


def GetISSNMatcher():
    return re.compile('([0-9]{4}-[0-9]{3}[0-9X])')


def ExportPersonalAuthenticationToken():
    config = util.LoadConfigFile("/usr/local/var/lib/tuelib/cronjobs/github_api_util.conf", no_error=True)
    pat = config.get("Personal Authentication Token", "pat")
    os.environ['GITHUB_OAUTH_TOKEN']  = pat
