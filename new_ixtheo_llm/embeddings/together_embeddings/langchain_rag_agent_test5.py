import asyncio
import os
from langchain.tools import tool
from langchain.agents import create_agent
from langgraph.checkpoint.memory import InMemorySaver 
from langchain_together import (TogetherEmbeddings, ChatTogether)
from langchain_milvus import Milvus
from langchain.agents.middleware import SummarizationMiddleware
from langchain_openai import OpenAIEmbeddings

from langfuse import observe
from langfuse.langchain import CallbackHandler
from solr_retriever import SolrRetriever

langfuse_handler = CallbackHandler()

@observe
async def AgentTest():
     #embeddings = TogetherEmbeddings(
     #     model="Alibaba-NLP/gte-modernbert-base"
     #)

     embeddings = OpenAIEmbeddings(
        model="qwen3-embedding-8b",
        api_key=os.environ["SCW_SECRET_KEY"],
        base_url="https://api.scaleway.ai/v1/",
        max_retries=10,        # retry up to 10 times
        request_timeout=60,    # 60 seconds per request
     )
     
     vectorstore = Milvus(
           embedding_function=embeddings,
           connection_args={"uri": "./milvus_demo.db"},
           collection_name="marc_rag_demo"
     )
     
    # llm = ChatTogether(model="moonshotai/Kimi-K2-Instruct-0905")
    # llm = ChatTogether(model="moonshotai/Kimi-K2.5")
    # llm = ChatTogether(model="zai-org/GLM-4.7")
     llm = ChatTogether(model="zai-org/GLM-5")
    # llm = ChatTogether(model="MiniMaxAI/MiniMax-M2.5")
    # llm = ChatTogether(model="Qwen/Qwen3.5-397B-A17B")
     
     
#     @tool(response_format="content_and_artifact")
     @tool()
     def semantic_record_search(query: str):
         """Retrieve semantic search record results."""
         retrieved_docs = vectorstore.similarity_search(query, k=5)
         return retrieved_docs

     @tool()
     def keyword_record_search(query: str):
         """Retrieve keyword based search record results."""
         retrieved_docs = SolrRetriever(config_file='langchain_rag_config.ini')._get_plain_relevant_documents(query)
         return retrieved_docs 
        
     
     tools = [semantic_record_search, keyword_record_search]
     # If desired, specify custom instructions
     prompt = (
         "You are a consultant for questions about literature in theology and Study of Religion. You have access to tools that retrieve theological bibliographical records"
         "Use the tools to help answer user queries. Be concise and also output the PPN of a referenced works as in link ixtheo.de/Record/PPN"
         "Provide verifiable sources for any of your statements"
         "Refuse to answer any questions that are not related to theology and the study of Religion"
         "If unsure say so and request additional information"
     )

     checkpointer=InMemorySaver()
     #memory_summarizer=ChatTogether(model="Qwen/Qwen2.5-32B")
     #memory_summarizer=ChatTogether(model="Qwen/Qwen3-Next-80B-A3B-Instruct")
     memory_summarizer=llm
     agent = create_agent(llm, tools, system_prompt=prompt, 
                          middleware=[
                             SummarizationMiddleware(
                                 model=memory_summarizer,
                                 trigger=("tokens", 90000),
                                 keep=("messages", 2)
                             )
                          ], checkpointer=checkpointer
     )

     

     while True:
         try:
             # Read question from user input
             question = input("\nEnter your question: ").strip()

             if question.lower() in ['quit', 'exit', 'q']:
                 print("Goodbye!")
                 break

             if not question:
                 print("Please enter a valid question.")
                 continue

             async for event in agent.astream(
                {"messages": [{"role": "user", "content": question}]},
                stream_mode="updates", config={"callbacks": [langfuse_handler], "configurable": {"thread_id": "1"}}
             ):
                for node, data in event.items():
                  if data is None:
                     continue
                  if 'messages' in data:
                      msg = data['messages'][-1]
                      print(msg.content, end="", flush=True)


         except KeyboardInterrupt:
             print("\n\nLoop aborted by user (Ctrl+C)")
             break
         except Exception as e:
             print(f"Error: {str(e)}")
             print("Continuing loop...")


if __name__ == "__main__":
    try:
        asyncio.run(AgentTest())
    except (KeyboardInterrupt, asyncio.CancelledError):
        print("Interrupted by user")

