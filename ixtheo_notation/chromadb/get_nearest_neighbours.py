import chromadb
import json
import os
from chromadb.utils.embedding_functions import TogetherAIEmbeddingFunction
import requests
import jq
import sys
import util


def Usage():
    print("Usage: " + sys.argv[0] + " ppn")
    sys.exit(1)


def GetConfig():
    return util.LoadConfigFile(os.path.basename(sys.argv[0])[:-2] + "conf")


def GetRecordData(ppn):
    solr  = requests.get("http://" + config.get("Solr", "server_and_port") + "/solr/biblio/select?fl=*&q.op=OR&q=id%3A" + ppn)
    return jq.compile('''.response.docs[] | walk(if . == ["[Unassigned]"] then null else . end) |
                 { id, title_full, author, topic_standardized, topic_non_standardized,
                 ixtheo_notation, era_facet, topic_facet, summary : ((.fullrecord | fromjson | .fields[]
                 | to_entries[] | select(.key=="520") | .value?.subfields[]?.a) // null)}''').input_value(solr.json()).first()


if len(sys.argv) != 2:
       Usage()

ppn = sys.argv[1]
config = GetConfig()
#client = chromadb.HttpClient(host='localhost', port=8000)
client = chromadb.PersistentClient(config.get("Chroma", "path"))
model_name=config.get("Embedding", "model")
embedding_function = TogetherAIEmbeddingFunction(
    model_name=model_name
)
collection = client.get_collection(config.get("Chroma", "collection"), embedding_function=embedding_function)
sys.stderr.write("Selection from " + str(collection.count()) + " records altogether")

record_data = GetRecordData(ppn)
results = collection.query(
#        query_texts=["Get similar records with respect to title and keywords " +  json.dumps(record_data)],
        query_texts=[json.dumps(record_data)],
    n_results=10,
)

print(json.dumps(results))

