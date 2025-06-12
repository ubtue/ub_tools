# Some parts with the use of AI
import chromadb
import json
import os
from chromadb.utils.embedding_functions import TogetherAIEmbeddingFunction
import requests
import pyjq
import sys
import util

def Usage():
    print("Usage: " + sys.argv[0] + " ppn_file")
    sys.exit(1)


def GetConfig():
    return util.LoadConfigFile(os.path.basename(sys.argv[0])[:-2] + "conf")


def GetRecordData(ppn):
    solr  = requests.get("http://" + config.get("Solr", "server_and_port") + "/solr/biblio/select?fl=*&q.op=OR&q=id%3A" + ppn)
    return pyjq.first('''.response.docs[] | { id, title_full, author, topic_standardized, topic_non_standardized,
                 ixtheo_notation, era_facet, topic_facet, summary : ((.fullrecord | fromjson | .fields[]
                 | to_entries[] | select(.key=="520") | .value?.subfields[]?.a) // null)}''',  solr.json())


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
client = chromadb.PersistentClient(config.get("Chroma", "path"))
model_name=config.get("Embedding", "model")
embedding_function = TogetherAIEmbeddingFunction(
   model_name=model_name
)
collection = client.get_or_create_collection(config.get("Chroma", "collection"), embedding_function=embedding_function)

ppns = ReadPPNFile(ppn_file)
batch_size = int(config.get("Chroma", "import_batch_size"))
for i in range(0, len(ppns), batch_size):
    ppn_batch = ppns[i:i+batch_size]
    ppn_batch_docs = list(map(json.dumps, (map(GetRecordData, ppn_batch))))
    print(ppn_batch)
    print(ppn_batch_docs)
    print("-----------------")
    collection.add(documents=ppn_batch_docs, ids=ppn_batch)
     

print("Finished...")
