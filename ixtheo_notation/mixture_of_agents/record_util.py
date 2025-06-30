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
    return jq.compile(''' . | del(.ixtheo_notation) ''').input_value(GetRecordData(ppn)).first()


def GetFulltext(ppn, text_type):
    text_type_num = ft_type_to_num[text_type];
    query = f"""{{"query" : {{ "bool" : {{ "must" : [ {{ "match" :  {{ "id" : "{ppn}" }} }}, {{ "match" : {{ "text_type" : {text_type_num} }} }} ] }} }} }}"""
    elasticsearch = requests.post("http://" + config.get("Elasticsearch", "server_and_port") + "/full_text_cache/_search", json=json.loads(query))
    return jq.compile('''.hits.hits[]._source | select(.is_publisher_provided == "false")  | .full_text''').input_value(elasticsearch.json()).first()
    

def GetTOC(ppn):
    return GetFulltext(ppn, "Table of Contents")


def GetRecordDataWithTOC(ppn):
    record=GetRecordData(ppn)
    if 'fulltext_types' in record and record['fulltext_types'] is not None and 'Table of Contents' in record['fulltext_types']:
        toc = jq.compile('@json').input(GetTOC(ppn)).first()
        return jq.compile('''. + {toc:''' + (toc) + '''}''').input_value(record).first()
    return record

def GetRecordDataWithTOCWithoutIxTheoNotation(ppn):
    return jq.compile(''' . | del(.ixtheo_notation) ''').input_value(GetRecordDataWithTOC(ppn)).first()


config = GetConfig()



