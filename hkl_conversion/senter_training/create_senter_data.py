#!/usr/bin/python

import spacy
import os
import random
import re

# tqdm is a great progress bar for python
# tqdm.auto automatically selects a text based progress for the console 
# and html based output in jupyter notebooks
#from tqdm.auto import tqdm

# DocBin is spacys new way to store Docs in a binary format for training later
from spacy.tokens import Doc
from spacy.tokens import DocBin

#nlp = spacy.blank("xx")
nlp = spacy.load("xx_sent_ud_sm")
#nlp = spacy.load("de_dep_news_trf")
#nlp.from_disk("training/spacy_pipeline_trf")

def make_docs(reference_dir, upper_half):
    docs = []
    lines = []
    for filename in os.scandir(reference_dir):
        if not filename.is_file():
            continue
        if filename.name.startswith('.'):
            continue
        with open(filename, 'r') as f:
            print("File: " + filename.name)
            lines += f.readlines()
            f.close()
    for i in range (1,3):
        random.shuffle(lines)
        sent_docs = []
        half_offset = int(len(lines) / 2)
        lines = lines[: half_offset ] if upper_half else lines[half_offset:-1]
        for line in lines:
            sent_doc = nlp(line)
            sent_doc[0].is_sent_start = True
            #for token in sent_doc[1:-1]:
            #    token.is_sent_start = False
            for token in sent_doc:
                if token.text == "//":
                    sent_doc[token.i].is_sent_start = True
                if token.text == "--":
                    sent_doc[token.i].is_sent_start = True
                if re.match(r'n[XVIL]+', token.text):
                    sent_doc[token.i].is_sent_start = True
            sent_docs.append(sent_doc)
        #for token in sent_docs:
        #    if token.is_sent_start:
        #        print("SENT_START XX: ")
        #        print(token)
        doc = Doc.from_docs(sent_docs, ensure_whitespace=True)
        docs.append(doc)
    return docs

# first we need to transform all the training data
train_reference_dir="training"
train_docs = make_docs(train_reference_dir, False)
# then we save it in a binary file to disc
doc_bin = DocBin(docs=train_docs)
doc_bin.to_disk("./data/senter_train.spacy")

# repeat for validation data
valid_docs = make_docs(train_reference_dir, True)
doc_bin = DocBin(docs=valid_docs)
doc_bin.to_disk("./data/senter_valid.spacy")
