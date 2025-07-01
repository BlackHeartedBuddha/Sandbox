# My Python Vibe Coding fun Projects

## RAG Model

```bash
# setup
source venv/bin/activate

pip install -r requirements.txt

python RAG/main.py
```

| Component  | Role                        | Example Tool           |
|------------|-----------------------------|-------------------------|
| Corpus     | Knowledge base              | Plain text or chunks    |
| Embedder   | Semantic vector converter   | SentenceTransformer     |
| Index      | Fast similarity search      | FAISS                   |
| Retriever  | Finds similar text to query | FAISS + embedder        |
| Generator  | Produces natural answer     | T5-small, BART          |


