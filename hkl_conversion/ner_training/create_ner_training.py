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

nlp = spacy.load("xx_sent_ud_sm")
extract_pattern=r'ner_all.spacy$'

def make_docs(reference_dir, pattern):
    all_doc_bins = DocBin()
    for filename in os.scandir(reference_dir):
        if not filename.is_file():
            continue
        if filename.name.startswith('.'):
            continue
        if not filename.name.endswith('.spacy'):
            continue
        if not re.match(extract_pattern, filename.name):
            continue
        print("Opening: " + filename.path)
        all_doc_bins.merge(DocBin().from_disk(filename.path))
    
    return list(all_doc_bins.get_docs(nlp.vocab))

## first we need to transform all the training data
train_reference_dir="training/converted"
docs_list = make_docs(train_reference_dir, extract_pattern)
random.shuffle(docs_list)
print("length: " + str(len(docs_list)))
training_size = int(len(docs_list)/5*4)
print("training_size: " + str(training_size))
train_docs = docs_list[:training_size]
valid_docs = docs_list[training_size + 1:]
doc_bin = DocBin(docs=train_docs)
doc_bin.to_disk("./training/ner_train.spacy")
doc_bin = DocBin(docs=valid_docs)
doc_bin.to_disk("./training/ner_valid.spacy")
