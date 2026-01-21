# Based on AI generated code
import asyncio
import os
import sys
from langchain_core.documents import Document
from langchain_text_splitters import RecursiveCharacterTextSplitter,RecursiveJsonSplitter
from langchain_together import TogetherEmbeddings
from langchain_milvus import Milvus
from langfuse import observe
from solr_retriever import SolrRetriever, SolrBatchRetriever
from langfuse import get_client
from langfuse.langchain import CallbackHandler
from pymilvus import MilvusClient
from transformers import AutoTokenizer

embedding_model = "Alibaba-NLP/gte-modernbert-base"
tokenizer = AutoTokenizer.from_pretrained(embedding_model)


def TruncateTokens(doc : Document, max_tokens=8000):
    ids = tokenizer.encode(doc.page_content, add_special_tokens=False)
    ids = ids[:max_tokens]
    doc.page_content = tokenizer.decode(ids, skip_special_tokens=True, clean_up_tokenization_spaces=True)


async def DropCollection(milvus_connection, collection_name):
    milvus_client=MilvusAsyncClient(**milvus_connection)
    if await milvus_client.has_collection(collection_name):
        await milvus_client.drop_collection(collection_name)


#solr_retriever = SolrRetriever()
#docs = solr_retriever._get_relevant_documents("*:*")
@observe()
async def GetSolrDocuments():
  lf = get_client()
  callback = CallbackHandler()
  collection_name="marc_rag_demo"
  milvus_connection={"uri" : "./milvus_demo.db"}
  DropCollection(milvus_connection, collection_name)
  solr_retriever = SolrBatchRetriever('-id:L*')
  #solr_retriever = SolrBatchRetriever('id:89891163X')
  while True:
     docs = solr_retriever.get_next_batch()
     if not docs:
         break

     for _ in map(TruncateTokens, docs):
         pass   # forces execution; docs are now updated


     embeddings = TogetherEmbeddings(
        model=embedding_model
      )

     print(docs)
     # Connect to Milvus (local standalone)
     vectorstore = Milvus.from_documents(
         documents=docs,
         embedding=embeddings,
         connection_args={"uri": "./milvus_demo.db"},  # Milvus Lite file
         enable_dynamic_field=True,
         collection_name=collection_name,
         drop_old=False,  # Clear existing data
     )


if __name__ == "__main__":
    asyncio.run(GetSolrDocuments())
