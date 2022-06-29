# Python 3 module
# -*- coding: utf-8 -*-
import re
import regex
import sys
import nltk, re, pprint
from nltk.corpus import state_union
from nltk.tokenize.punkt import PunktSentenceTokenizer, PunktTrainer, PunktParameters
nltk.download('stopwords')
import random
from langdetect import detect
from dicttoxml import dicttoxml
import json
from boltons.iterutils import remap

import jsonpickle
from functools import reduce

def ContainsProbableEditionYear(sentence):
    return bool(re.search(r'\s+(1[789]\d{2}|20[01][0-9])\b', sentence))


def ContainsCf(sentence):
    return bool(re.search(r'\bCf\b', sentence))


def ContainsCapitalAbbreviation(sentence):
    return bool(re.search(r'\b.[A-Z]+\b', sentence))


def ContainsPageSequence(sentence):
    return bool(re.search(r'\b\d+(ff?[.])\b', sentence))


class GetFrequentlyUsedWordFeatures:
    def __init__(self, traindata):
        all_words = nltk.FreqDist(w for w in nltk.word_tokenize(' '.join(traindata)))
        self.most_frequent_words_ = list(all_words)[:1000]
    def __call__(self):
        if self.most_frequent_words_ == {}:
            raise Exception("Uninitialized frequencies")
        return self.most_frequent_words_


def GetWordFeatures(word_selection, sentence):
    features = {}
    sentence_words = set(nltk.word_tokenize(sentence))
    for word in word_selection:
        features['contains({})'.format(word)] = (word in sentence_words)
    return features


def ExtractFeatures(sentence):
    try:
        lang = detect(sentence)
    except Exception as e:
        lang = 'Unknown'

    features = { 'lang' : lang, 'length': len(nltk.word_tokenize(sentence)), 'edition_years': ContainsProbableEditionYear(sentence),
                 'contains_cf' : ContainsCf(sentence),  'contains_cap_abbrev' : ContainsCapitalAbbreviation(sentence),
                 'contains_page_seq' : ContainsPageSequence(sentence),  **GetWordFeatures(word_selection(), sentence) }
    return features


def SetupWordFeatures(labeled):
    all_trainsentences = list(map(lambda pair: pair[0], labeled))
    global word_selection
    word_selection = GetFrequentlyUsedWordFeatures(all_trainsentences)


SENTENCE_TYPES = { 'TITLE' : 'title', 'BIB_INFO' : 'bib_info', 'YEAR_AND_PLACE' : 'year_and_place', 'COMMENT' : 'comment' }

def CreateClassifier():
    with open("./training/titles.txt") as titles:
       titles = [ (title, SENTENCE_TYPES['TITLE']) for title in titles ]
    with open("./training/comments.txt") as comments:
       comments = [ (comment, SENTENCE_TYPES['COMMENT']) for comment in comments ]
    with open("./training/bib_infos.txt") as bib_infos:
       bib_infos = [ (bib_info, SENTENCE_TYPES['BIB_INFO']) for bib_info in bib_infos ]
    with open("./training/years_and_places.txt") as years_and_places:
       years_and_places = [ (year_and_place, SENTENCE_TYPES['YEAR_AND_PLACE']) for year_and_place in years_and_places ]
    labeled = titles + comments + bib_infos + years_and_places
    SetupWordFeatures(labeled)
    random.shuffle(labeled)
    training = [(ExtractFeatures(example), sentence_type) for (example, sentence_type) in labeled]
    return nltk.NaiveBayesClassifier.train(training)

