# Python 3 module
# -*- coding: utf-8 -*-
import json
import jq
import os
import requests
import sys
import util


ft_type_to_num = { "Table of Contents" : 2, "Fulltext" : 1 }

def GetConfig():
    return util.LoadConfigFile(os.path.basename(sys.argv[0])[:-2] + "conf")


def GetRecordData(ppn):
    solr  = requests.get("http://" + config.get("Solr", "server_and_port") + "/solr/biblio/select?fl=*&q.op=OR&q=id%3A" + ppn)
    return jq.compile('''.response.docs[] | walk(if . == ["[Unassigned]"] then null else . end) |
                 { id, title_full, author, topic_standardized, topic_non_standardized,
                 ixtheo_notation, era_facet, topic_facet, summary : ((.fullrecord | fromjson | .fields[]
                 | to_entries[] | select(.key=="520") | .value?.subfields[]?.a) // null), fulltext_types, has_publisher_fulltext}''').input_value(solr.json()).first()


def GetRecordDataWithoutIxTheoNotation(ppn):
    solr  = requests.get("http://" + config.get("Solr", "server_and_port") + "/solr/biblio/select?fl=*&q.op=OR&q=id%3A" + ppn)
    return jq.compile('''.response.docs[] | walk(if . == ["[Unassigned]"] then null else . end) |
                 { id, title_full, author, topic_standardized, topic_non_standardized,
                 era_facet, topic_facet, summary : ((.fullrecord | fromjson | .fields[]
                 | to_entries[] | select(.key=="520") | .value?.subfields[]?.a) // null)}''').input_value(solr.json()).first()


def GetFulltext(ppn, text_type):
    text_type_num = ft_type_to_num[text_type];
    query = f"""{{"query" : {{ "bool" : {{ "must" : [ {{ "match" :  {{ "id" : "{ppn}" }} }}, {{ "match" : {{ "text_type" : {text_type_num} }} }} ] }} }} }}"""
    elasticsearch = requests.post("http://" + config.get("Elasticsearch", "server_and_port") + "/full_text_cache/_search", json=json.loads(query))
    return jq.compile('''.''').input_value(elasticsearch.json()).first()
    

def GetTOC(ppn):
    return GetFulltext(ppn, "Table of Contents")

config = GetConfig()



