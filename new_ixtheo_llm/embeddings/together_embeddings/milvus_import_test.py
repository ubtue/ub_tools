# Based on AI generated code
import os
import sys
from langchain_core.documents import Document
from langchain_text_splitters import RecursiveCharacterTextSplitter,RecursiveJsonSplitter
from langchain_together import TogetherEmbeddings
from langchain_milvus import Milvus
from solr_retriever import SolrRetriever, SolrBatchRetriever


#solr_retriever = SolrRetriever()
#docs = solr_retriever._get_relevant_documents("*:*")
solr_retriever = SolrBatchRetriever('-id:L*')
while True:
  docs = solr_retriever.get_next_batch()
  if not docs:
           break

  embeddings = TogetherEmbeddings(
     model="Alibaba-NLP/gte-modernbert-base"
   )

  # Connect to Milvus (local standalone)
  vectorstore = Milvus.from_documents(
      documents=docs,
      embedding=embeddings,
      connection_args={"uri": "./milvus_demo.db"},  # Milvus Lite file
      enable_dynamic_field=True,
      collection_name="marc_rag_demo",
      drop_old=True,  # Clear existing data
  )
