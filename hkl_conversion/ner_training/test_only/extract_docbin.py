#!/usr/bin/python3

import spacy
from spacy.tokens import DocBin

print("HALLO")
nlp = spacy.blank("en")
#doc_bin = DocBin().from_disk("./data/train1.spacy")
#doc_bin = DocBin().from_disk("training/ner_train.spacy")
doc_bin = DocBin().from_disk("training/ner_valid.spacy")
docs = doc_bin.get_docs(nlp.vocab)
i=0
for doc in docs:
    for sent in doc.sents:
        print(sent.text)
        print("\n++++++++++++++++++++++++++++++++++\n")
    for ent in doc.ents:
        print(ent.text)
        print("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n")
#    print(doc.ents)
    print('\n##############################################\n')
#        ++i
#    if i > 10:
#        break


