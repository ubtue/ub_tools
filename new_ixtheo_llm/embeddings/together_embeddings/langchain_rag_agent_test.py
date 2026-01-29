import asyncio
from langchain.tools import tool
from langchain.agents import create_agent
from langchain_together import (TogetherEmbeddings, ChatTogether)
from langchain_milvus import Milvus

from langfuse import observe
from langfuse.langchain import CallbackHandler

langfuse_handler = CallbackHandler()

@observe
async def AgentTest():
     embeddings = TogetherEmbeddings(
          model="Alibaba-NLP/gte-modernbert-base"
     )
     
     vectorstore = Milvus(
           embedding_function=embeddings,
           connection_args={"uri": "./milvus_demo.db"},
           collection_name="marc_rag_demo"
     )
     
    # llm = ChatTogether(model="moonshotai/Kimi-K2-Instruct-0905")
    # llm = ChatTogether(model="moonshotai/Kimi-K2.5")
     llm = ChatTogether(model="zai-org/GLM-4.7")
     
     
     @tool(response_format="content_and_artifact")
     def retrieve_context(query: str):
         """Retrieve information to help answer a query."""
         retrieved_docs = vectorstore.similarity_search(query, k=5)
         serialized = "\n\n".join(
             (f"Source: {doc.metadata}\nContent: {doc.page_content}")
             for doc in retrieved_docs
         )
         return serialized, retrieved_docs
     
     
     tools = [retrieve_context]
     # If desired, specify custom instructions
     prompt = (
         "You are a consultant for questions about literature in theology and Study of Religion. You have access to a tool that retrieves theological bibliographical records"
         "Use the tool to help answer user queries. Be concise and also output the PPN of a referenced works"
     )
     agent = create_agent(llm, tools, system_prompt=prompt)

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
                stream_mode="updates", config={"callbacks": [langfuse_handler]}
             ):
                for node, data in event.items():
                  if 'messages' in data:
                      msg = data['messages'][-1]
                      print(msg.content, end="")




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

