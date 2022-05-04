#!/usr/bin/python3

import re
import regex
import sys
import nltk, re, pprint
import random
from langdetect import detect
from dicttoxml import dicttoxml
import json
from boltons.iterutils import remap

import jsonpickle
from functools import reduce

import spacy
from spacy import displacy
from spacy.lang.en import English
from spacy.language import Language
from spacy.matcher import PhraseMatcher

from stanza.pipeline.processor import ProcessorVariant, register_processor_variant, Processor, register_processor


import stanza
import spacy_stanza
#from pysbd.utils import PySBDFactory

def SplitToAuthorEntries(file):
    entry = []
    entries = []
    author = ''
    author_match_regex = regex.compile(r'^\p{Lu}\p{Ll}+\s*,(\s*\p{Lu}([.]|\p{Ll})+)+$')
    for line in file:
        if author_match_regex.match(line):
            if author == '':
                author = line
                continue
            else:
                entries.append((author, entry))
                author = line
                entry = []
                continue
        entry.append(line)
    return entries


def ReduceMultipleEmptyLinesToOne(file):
    return re.sub(r'(\n\s*)+\n', '\n\n', file);


def GetBufferLikeFile(file):
    return (line + '\n' for line in file.split('\n'))


def FilterPageHeadings(file):
    return re.sub(r'(?:^\n{2}\d+\n{2}[^,]+\n{3})|(?:^\n{2}\w+\n{3}\d+\n{3})|(?:^\n{2}\d+\n{3})', '\n\n', file, flags=re.MULTILINE)


def ContainsProbableEditionYear(sentence):
    return bool(re.search(r'\s+(1[789]\d{2}|20[01][0-9])\b', sentence))


def ContainsCf(sentence):
    return bool(re.search(r'\bCf\b', sentence))


def ContainsCapitalAbbreviation(sentence):
    return bool(re.search(r'\b.[A-Z]+\b', sentence))


def ContainsPageSequence(sentence):
    return bool(re.search(r'\b\d+(ff?[.])\b', sentence))


@Language.component("set_custom_boundaries")
def set_custom_boundaries(doc):
    #doc.is_parsed = False
    for token in doc[:-1]:
        if token.text == "//":
            doc[token.i].is_sent_start = True
        if token.text == "--":
            doc[token.i].is_sent_start = True
        if re.match(r'n[XVIL]+', token.text):
            doc[token.i].is_sent_start = True
    return doc


def Main():
    try:
         if len(sys.argv) != 2:
             print("Invalid arguments: usage " + sys.argv[0] + " hkl_extraction_file")
             sys.exit(-1)
         with open(sys.argv[1]) as f:
             file = FilterPageHeadings(f.read())
             entries = SplitToAuthorEntries(GetBufferLikeFile(file))
             authors = []
             stanza.download("en")
             nlp = spacy_stanza.load_pipeline("en", processors="tokenize,pos,lemma,depparse" )
             matcher = PhraseMatcher(nlp.vocab)
             terms = ["Zweispr. Kultlied", "Sumer. Kultlied.", "Zweispr. Hymnus."]
             patterns = [nlp.make_doc(text) for text in terms]
             matcher.add("TerminologyList", patterns)
             analysis = nlp.analyze_pipes(pretty=True)
             nlp.to_disk("./spacy_pipeline_stanza")
             for author, entry in entries:
                 print("---------------------------------------\nAUTHOR: " + author + '\n')
                 normalized_separations = re.sub(r'-\n', '', ''.join(entry))
                 normalized_newlines= re.sub(r'\n(?!\n)', ' ', normalized_separations)
                 sentences = []
                 for to_parse in [ normalized_newlines ]:
                     print("XXX " + to_parse + '\n#########################################\n')
                     pre_nlp = spacy.blank("en")
                     pre_nlp.add_pipe("set_custom_boundaries")
                     pre_parsed_doc = pre_nlp(to_parse);
                     for pre_parsed_sentence in pre_parsed_doc.sents:
                         doc = nlp(pre_parsed_sentence.orth_.strip())
                         for sentence in doc.sents:
                             sentence = sentence.orth_.strip()
                             sentences.append(sentence)
                            # print("SENT: " + sentence)
                     new_sentences = []
                     for sentence in sentences:
                         index = sentences.index(sentence)
                         #print(str(index) + ": " + sentence)
                         if re.search(r'Zweispr.$', sentence) and index < len(sentences) - 1:
                             new_sentences.append(reduce(lambda sentx, senty: sentx + " " + senty, sentences[index : index + 2]))
                         else:
                             new_sentences.append(sentence)
                     for sentence in new_sentences:
                         print("SENT: " + sentence)

    except Exception as e:
        print("ERROR: " + e)



if __name__ == "__main__":
    Main()
