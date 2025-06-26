# Some parts with the use of AI
import sys
sys.path.append('../')
import chromadb
import json
import os
import requests
import jq
import util

def Usage():
    print("Usage: " + sys.argv[0] + " ppn_file")
    sys.exit(1)


def GetConfig():
    return util.LoadConfigFile(os.path.basename(sys.argv[0])[:-2] + "conf")


def GetRecordData(ppn):
    solr  = requests.get("http://" + config.get("Solr", "server_and_port") + "/solr/biblio/select?fl=*&q.op=OR&q=id%3A" + ppn)
    return jq.compile('''.response.docs[] | walk(if . == ["[Unassigned]"] then null else . end) | { record :
                 { id, title_full, author, topic_standardized, topic_non_standardized,
                  era_facet, topic_facet, summary : ((.fullrecord | fromjson | .fields[]
                 | to_entries[] | select(.key=="520") | .value?.subfields[]?.a) // null)} , correct_answer : .ixtheo_notation}''').input_value(solr.json()).first()


def ReadPPNFile(ppn_file):
    ppns = []
    try:
        with open(ppn_file, 'r') as file:
            lines = file.readlines()
            for ppn in lines:
                ppns.append(ppn.strip())
    except FileNotFoundError:
        print("Error: " + file_path + " not found")
    except Exception as e:
        print(f"An error occurred: {e}")

    return ppns


if len(sys.argv) != 2:
       Usage() 

ppn_file = sys.argv[1]
config=GetConfig()
ppns = ReadPPNFile(ppn_file)
batch_size=int(config.get("Batch", "batch_size"))
with open("output1.txt", "a") as f:
    for i in range(0, len(ppns), batch_size):
        ppn_batch = ppns[i:i+batch_size]
        ppn_batch_docs = json.dumps(list(map(GetRecordData, ppn_batch)))
        print(ppn_batch)
        print(ppn_batch_docs, file=f)
        print("-----------------")

print("Finished...")
