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
import nltk_classification
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
    # Make sure we have sents
    if not doc.has_annotation("SENT_START"):
        #print ("NO SENT START")
        doc[-1].is_sent_start = True
    return doc


abbreviations_for_line_merge = r'(Zweispr.|Sumer.|assyr.|Neuassyr.|vol.|=)$'


def SplitToAuthorSentenceGroupsAndSentences(entries):
      authors_and_sentence_groups = []
      stanza.download("en")
      nlp = spacy_stanza.load_pipeline("en", processors="tokenize,pos,lemma,depparse" )
      matcher = PhraseMatcher(nlp.vocab)
      terms = ["Zweispr. Kultlied", "Sumer. Kultlied.", "Zweispr. Hymnus.", "vol. II."]
      patterns = [nlp.make_doc(text) for text in terms]
      matcher.add("TerminologyList", patterns)
      analysis = nlp.analyze_pipes(pretty=True)
      nlp.to_disk("./spacy_pipeline_stanza")
      for author, entry in entries:
          normalized_separations = re.sub(r'-\n', '', ''.join(entry))
          normalized_newlines= re.sub(r'\n(?!\n)', ' ', normalized_separations)
          sentences = []
          sentence_groups = []
          print("AUTHOR: " + author)
          for to_parse in [ normalized_newlines ]:
              print("ORIG: " + to_parse)
              for to_parse_single in re.split(r'\n\n',to_parse):
                  pre_nlp = spacy.blank("en")
                  pre_nlp.add_pipe("set_custom_boundaries")
                  pre_parsed_doc = pre_nlp(to_parse_single);
                  for pre_parsed_sentence in pre_parsed_doc.sents:
                      doc = nlp(pre_parsed_sentence.orth_.strip())
                      for sentence in doc.sents:
                          sentence = sentence.orth_.strip()
                          sentences.append(sentence)
                  new_sentences = []
                  for sentence in sentences:
                      index = sentences.index(sentence)
                      if re.search(abbreviations_for_line_merge, sentence) and index < len(sentences) - 1:
                          sentences[index : index + 2] = [reduce(lambda sentx, senty: sentx + " " + senty, sentences[index : index + 2])]
                  sentence_groups.append(sentences)
                  sentences = []
          authors_and_sentence_groups.append((author, sentence_groups))
      return authors_and_sentence_groups


def PrintResultsRaw(authors_and_sentence_groups):
    for author, sentence_groups in authors_and_sentence_groups:
        print("---------------------------------------\nAUTHOR: " + author + '\n')
        for sentences in sentence_groups:
             print("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%")
             for sentence in sentences:
                 print("SENT: " + sentence)

def HandleMissingTitle(author_tree):
    author_tree['titles'].append( { 'title' : 'MISATTRIBUTED TITLE (!!)',  'elements' : []  } )
    return author_tree


def GetHighestLabel(cats):
    return list(sorted(cats, key=cats.get, reverse=True))[0]


def ClassifySentences(authors_and_sentence_groups, output):
    #classifier = nltk_classification.CreateClassifier()
    classifier = spacy.load("training/output/model-best")
    authors = []
    author_tree = dict()

    for author, sentence_groups in authors_and_sentence_groups:
        print("---------------------------------------\nAUTHOR: " + author + '\n')
        author_tree['author'] = author
        author_tree['titles'] = []
        for sentences in sentence_groups:
             print("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%")
             for sentence in sentences:
                 sentence_type = GetHighestLabel(classifier(sentence).cats).upper()
                 print("SENTENCE TYPE: " + sentence_type)
                 if sentence_type == 'TITLE':
                      title = sentence
                      author_tree['titles'].append({ 'title' : title, 'elements' : [] })
                 elif sentence_type == 'YEAR_AND_PLACE':
                      if not author_tree['titles']:
                          author_tree = HandleMissingTitle(author_tree)
                      author_tree['titles'][-1]['elements'].append( {'year_and_place' :  sentence })
                 elif sentence_type == 'BIB_INFO':
                     if not author_tree['titles']:
                         author_tree = HandleMissingTitle(author_tree)
                     author_tree['titles'][-1]['elements'].append( { 'bib_info' : sentence })
                 elif sentence_type == 'COMMENT':
                     if not author_tree['titles']:
                         author_tree = HandleMissingTitle(author_tree)
                     author_tree['titles'][-1]['elements'].append({ 'comment' : sentence})
                 elif sentence_type == 'INTERNAL_REFERENCE':
                     if not author_tree['titles']:
                         author_tree = HandleMissingTitle(author_tree)
                     author_tree['titles'][-1]['elements'].append({ 'internal_reference' : sentence })

        drop_falsey = lambda path, key, value: bool(value)
        authors.append(remap(author_tree, visit=drop_falsey))
    with open(output, 'w') as json_out:
        json.dump(authors, json_out)




def Main():
    try:
         if len(sys.argv) != 3:
             print("Invalid arguments: usage " + sys.argv[0] + " hkl_extraction_file output.json")
             sys.exit(-1)
         with open(sys.argv[1]) as f:
             file = FilterPageHeadings(f.read())
             entries = SplitToAuthorEntries(GetBufferLikeFile(file))
             authors_and_sentence_groups = SplitToAuthorSentenceGroupsAndSentences(entries)
             ClassifySentences(authors_and_sentence_groups, sys.argv[2])




    except Exception as e:
        print("ERROR: " + e)



if __name__ == "__main__":
    Main()
