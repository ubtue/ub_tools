#!/bin/python3
# -*- coding: utf-8 -*-
import pyjq
import json
import sys
import util

    
def ReadFulltext(file_path):
    with open(file_path) as json_data:
        fulltext_parsed = json.load(json_data)
        return fulltext_parsed


def GetSolrRecord(solr_host, id):
    return pyjq.all('.response.docs[]', url='http://' +  solr_host + ':8983/solr/biblio/select?fl=*&indent=true&q.op=OR&q=id%3A' + 
             id + '&useParams=')



def GetSolrTitle(solr_record):
    return pyjq.first('.[].title', solr_record)


def GetSolrKeywordChains(solr_record):
    return pyjq.first('.[].key_word_chains_en', solr_record)


def AddSolrInformation(solr_host, json_data):
        for item in json_data:
            record = GetSolrRecord(solr_host, item['id'])
            title = GetSolrTitle(record)
            if title is None:
                continue
            item['title'] = title

            keyword_chains = GetSolrKeywordChains(record)
            if keyword_chains is None:
                 continue
            item['keyword_chains'] = keyword_chains


def Main():
    if len(sys.argv) != 3:
       print("Usage: " + sys.argv[0] + " fulltext.json solr_server")
       exit(1)
    fulltext_dump_file = sys.argv[1];
    solr_host = sys.argv[2];
    fulltexts = pyjq.all('.hits.hits[]._source | { id, full_text }', ReadFulltext(fulltext_dump_file))
    AddSolrInformation(solr_host, fulltexts)
    print(json.dumps(fulltexts))


if __name__ == "__main__":
    Main()


