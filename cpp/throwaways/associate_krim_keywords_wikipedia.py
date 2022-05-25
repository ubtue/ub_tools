#!/usr/bin/python3
# Based on https://pypi.org/project/Wikipedia-API/ (220523)

import os
import re
import sys
import traceback
from typing import Dict, Any, List, Optional
import wikipediaapi

PagesDict = Dict[str, 'ubtue_WikipediaPage']


class ubtue_Wikipedia(wikipediaapi.Wikipedia):
    def __init__(
            self,
            language: str = 'en',
            extract_format: wikipediaapi.ExtractFormat = wikipediaapi.ExtractFormat.WIKI,
            headers: Optional[Dict[str, Any]] = None,
            **kwargs
    ) -> None:
         super(ubtue_Wikipedia, self).__init__(language, extract_format, headers, **kwargs)
    def extlinks(
            self,
            page: 'ubtue_WikipediaPage',
            **kwargs
    ) -> PagesDict:
        """
            Returns external links from other pages with respect to parameters
        """    
        params = {
                'action': 'query',
                'prop': 'extlinks',
                'titles': page.title,
                'ellimit': 500,
        }

        used_params = kwargs
        used_params.update(params)

        raw = self._query(
                page,
                used_params
        )
        #print(raw)
        self._common_attributes(raw['query'], page)
        v = raw['query']
        while 'continue' in raw:
             params['elcontinue'] = raw['continue']['elcontinue']
             raw = self._query(
                  page,
                  params
             )
             v['extlinks'] += raw['query']['extlinks']
        return self._build_extlinks(v, page)


    def _build_extlinks(
            self,
            extract,
            page
    ) -> PagesDict:
         page._extlinks = []
         pages = extract['pages']
         # Get away with pageid
         if len(pages.keys()) != 1:
            raise "Invalid number of elements in pages: " + len(pages.keys)
         pageid = list(pages.keys())[0]
         extlinks = pages[pageid].get('extlinks', [])
         self._common_attributes(extract, page)
         for extlink in extlinks:
             extlink_clean = list(extlink.values())[0]
             page._extlinks.append(extlink_clean)
         #print(page._extlinks)
         return page._extlinks    


class ubtue_WikipediaPage(object):
    def __init__(
            self,
            orig
    ) -> None:
        self.myorig = orig
        self._extlinks = {}
    def __getattr__(self, attr):
        return getattr(self.myorig, attr)
    @property
    def extlinks(self) -> PagesDict:
         return  self._fetch('extlinks')


def print_links(page):
        links = page.links
        for title in sorted(links.keys()):
            print("%s: %s" % (title, links[title]))


def print_sections(sections, level=0):
        for s in sections:
                print("%s: %s - %s" % ("*" * (level + 1), s.title, s.text[0:40]))
                print_sections(s.sections, level + 1)


def filterGNDLinks(extlinks):
    #print(extlinks)
    gnd_matcher = re.compile(r'^https://d-nb.info/gnd/(.*)$')
    return list(filter(gnd_matcher.match, extlinks))



def Main():
    if len(sys.argv) != 2:
       print ("Usage: " + sys.argv[0] + ": line_separated keyword file")
       sys.exit(1)
    kwfile = open(sys.argv[1], 'r')
    wiki_wiki = ubtue_Wikipedia('de')
    
    for line in kwfile:
        page_candidate = line.strip()
        page = ubtue_WikipediaPage(wiki_wiki.page(page_candidate))
        if page.exists():
            print("%s" % page_candidate, end=' | ')
            print(page.fullurl, end=' | ')
            #print("PAGEID " + str(page.pageid))
            gnd_links = filterGNDLinks(page.extlinks._extlinks)
            print(gnd_links)
           # print("#############################################")
        else:
            print ("%s - No Match" % page_candidate)
        sys.stdout.flush()

try:
    Main()
except Exception as e:
    error_msg =  "An unexpected error occurred: " + str(e) + "\n\n" + traceback.format_exc(20)
    sys.stderr.write(error_msg)
