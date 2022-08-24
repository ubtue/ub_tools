#!/usr/bin/python

import spacy
import os
from pathlib import Path

# tqdm is a great progress bar for python
# tqdm.auto automatically selects a text based progress for the console 
# and html based output in jupyter notebooks
from tqdm.auto import tqdm

# DocBin is spacys new way to store Docs in a binary format for training later
from spacy.tokens import DocBin

# load a medium sized english language model in spacy
nlp = spacy.load("en_core_web_lg")

def make_docs(reference_dir):
    docs = []
    for filename in os.scandir(reference_dir):
        if not filename.is_file():
            continue
        if filename.name.startswith('.'):
            continue
        with open(filename, 'r') as f:
            print("File: " + filename.name)
            lines = f.readlines()
            label = Path(filename.name).stem[:-1]
            # Normalize years_and_place to correct label
            label = "year_and_place" if label=="years_and_place" else label 
            print ("Label: " + label)
            for line in lines:
                doc = nlp(line)
                doc.cats[label] = 1.0
                docs.append(doc)
            f.close()
    return docs

# first we need to transform all the training data
train_reference_dir="../training"
train_docs = make_docs(train_reference_dir)
# then we save it in a binary file to disc
doc_bin = DocBin(docs=train_docs)
doc_bin.to_disk("./data/train.spacy")

# repeat for validation data
valid_docs = make_docs(train_reference_dir)
doc_bin = DocBin(docs=valid_docs)
doc_bin.to_disk("./data/valid.spacy")
