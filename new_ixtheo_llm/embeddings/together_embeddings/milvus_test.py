# Based on AI generated code

from langchain_together import ChatTogether
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.output_parsers import StrOutputParser
from langchain_core.runnables import RunnablePassthrough
from langchain_together import TogetherEmbeddings
from langchain_milvus import Milvus


# LLM for generation
#llm = ChatTogether(model="meta-llama/Llama-3.1-8B-Instruct")
#llm = ChatTogether(model="meta-llama/Llama-4-Maverick-17B-128E-Instruct-FP8")
llm = ChatTogether(model="moonshotai/Kimi-K2-Instruct-0905")

embeddings = TogetherEmbeddings(
    #model="together.ai/multilingual-e5-large-instruct"  # or "BAAI/bge-large-en-v1.5"
    model="Alibaba-NLP/gte-modernbert-base"
)


# Prompt template
prompt = ChatPromptTemplate.from_template(
    """Answer the question based on the context:
{context}
Question: {question}"""
)

vectorstore = Milvus(
    embedding_function=embeddings,
    connection_args={"uri": "./milvus_demo.db"},
    collection_name="marc_rag_demo"
)


# Retriever
retriever = vectorstore.as_retriever(search_kwargs={"k": 10})

# RAG chain using LCEL
def format_docs(docs):
    return "\n\n".join(doc.page_content for doc in docs)

rag_chain = (
    {"context": retriever | format_docs, "question": RunnablePassthrough()}
    | prompt
    | llm
    | StrOutputParser()
)

# Test query
#response = rag_chain.invoke("Welche Titel beschäftigen sich mit Kuba (mit PPN)")
#response = rag_chain.invoke("Welche Titel sind computerrerssourcen (mit PPN)")


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

        # Invoke the LangChain chain
        response = rag_chain.invoke(question)
        print(f"Answer: {response}")

    except KeyboardInterrupt:
        print("\n\nLoop aborted by user (Ctrl+C)")
        break
    except Exception as e:
        print(f"Error: {str(e)}")
        print("Continuing loop...")

