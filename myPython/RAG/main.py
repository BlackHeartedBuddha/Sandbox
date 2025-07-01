from sentence_transformers import SentenceTransformer
from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
import faiss
import torch

# Step 1: Corpus (document chunks)
documents = [
    "The Eiffel Tower is located in Paris.",
    "The Great Wall of China is visible from space.",
    "Water boils at 100 degrees Celsius.",
]

# Step 2: Encode documents
embedder = SentenceTransformer("all-MiniLM-L6-v2")
doc_embeddings = embedder.encode(documents, convert_to_tensor=False)

# Step 3: Create FAISS index
dim = doc_embeddings[0].shape[0]
index = faiss.IndexFlatL2(dim)
index.add(doc_embeddings)

# Step 4: Query
query = "Give me a liquid"
query_embedding = embedder.encode([query])

# Step 5: Retrieve top document
_, indices = index.search(query_embedding, k=1)
retrieved_doc = documents[indices[0][0]]

# Step 6: Load local generator (e.g., T5-small)
tokenizer = AutoTokenizer.from_pretrained("t5-small")
model = AutoModelForSeq2SeqLM.from_pretrained("t5-small")

# Step 7: Format prompt and generate answer
prompt = f"question: {query} context: {retrieved_doc}"
inputs = tokenizer(prompt, return_tensors="pt")
outputs = model.generate(**inputs, max_new_tokens=50)
answer = tokenizer.decode(outputs[0], skip_special_tokens=True)

print(f"Query: {query}")
print(f"Retrieved: {retrieved_doc}")
print(f"Answer: {answer}")
