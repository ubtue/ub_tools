#!/usr/bin/python3
# Based on https://pypi.org/project/Wikipedia-API/ (220523)

import os
import re
import sys
import time
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


    def extract_extlinks(self, query) -> Dict:
         pages = query['pages']
         pageid = self.get_first_page_id(pages)
         return pages[pageid].get('extlinks', [])


    def get_first_page_id(self, pages) -> str:
        # Get away with pageid
         if len(pages.keys()) != 1:
            raise "Invalid number of elements in pages: " + len(pages.keys)
         return list(pages.keys())[0]



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
             pageid = self.get_first_page_id(raw['query']['pages'])
             if 'extlinks' in raw['query']['pages'][pageid]:
                  v['pages'][pageid]['extlinks'] +=  raw['query']['pages'][pageid]['extlinks']
        return self._build_extlinks(v, page)


    def _build_extlinks(
            self,
            extract,
            page
    ) -> PagesDict:
         page._extlinks = []
         extlinks = self.extract_extlinks(extract)
         self._common_attributes(extract, page)
         for extlink in extlinks:
             extlink_clean = list(extlink.values())[0]
             page._extlinks.append(extlink_clean)
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
        time.sleep(2000/1000)


try:
    Main()
except Exception as e:
    error_msg =  "An unexpected error occurred: " + str(e) + "\n\n" + traceback.format_exc(20)
    sys.stderr.write(error_msg)
