from langchain_community.document_loaders import TextLoader
from langchain.text_splitter import RecursiveCharacterTextSplitter
from langchain.embeddings import HuggingFaceEmbeddings
from langchain.vectorstores import FAISS
from langchain.llms import HuggingFacePipeline
from langchain.chains import RetrievalQA
from langchain.prompts import PromptTemplate

from transformers import AutoTokenizer, AutoModelForSeq2SeqLM, pipeline

# === Step 1: Load documents ===
loader = TextLoader("doc.txt")  # Load local text file
documents = loader.load()
for doc in documents:
    doc.page_content = doc.page_content.strip()

# === Step 2: Split into smart chunks ===
splitter = RecursiveCharacterTextSplitter(
    chunk_size=500,
    chunk_overlap=100,
    separators=["\n\n", "\n", ".", " "]
)
docs = splitter.split_documents(documents)

# === Step 3: Embed with HuggingFace (no API needed) ===
embedding = HuggingFaceEmbeddings(model_name="all-MiniLM-L6-v2")

# === Step 4: Store vectors in FAISS ===
db = FAISS.from_documents(docs, embedding)

# === Step 5: Create retriever with MMR ===
retriever = db.as_retriever(
    search_type="mmr",
    search_kwargs={"k": 5, "fetch_k": 10}
)

# === Step 6: Load flan-t5-base LLM ===
print("Loading instruction-tuned local model (flan-t5-base)...")
model_name = "google/flan-t5-base"
tokenizer = AutoTokenizer.from_pretrained(model_name)
model = AutoModelForSeq2SeqLM.from_pretrained(model_name)

pipe = pipeline("text2text-generation", model=model, tokenizer=tokenizer, max_new_tokens=256)
llm = HuggingFacePipeline(pipeline=pipe)

# === Step 7: Custom prompt ===
prompt_template = """
You are a helpful assistant. Use the following context to answer the question. If the answer is not in the context, say "I don't know."

Context:
{context}

Question: {question}
Answer:"""

prompt = PromptTemplate(
    template=prompt_template,
    input_variables=["context", "question"]
)

# === Step 8: Build RetrievalQA chain with prompt ===
qa_chain = RetrievalQA.from_chain_type(
    llm=llm,
    retriever=retriever,
    chain_type="stuff",
    chain_type_kwargs={"prompt": prompt},
    return_source_documents=True
)

# === Step 9: Ask user ===
while True:
    query = input("\nAsk a question (or 'exit'): ")
    if query.lower() in ["exit", "quit"]:
        break

    result = qa_chain(query)
    print("\nAnswer:")
    print(result["result"])

    print("\nSources:")
    for i, doc in enumerate(result["source_documents"]):
        print(f"Source {i+1}:\n{doc.page_content[:300]}")
