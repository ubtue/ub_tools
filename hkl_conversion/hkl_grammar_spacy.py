#!/usr/bin/python3

import re
import regex
import sys
import nltk, re, pprint
nltk.download('punkt')
#nltk.download('averaged_perceptron_tagger')
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

import spacy
from spacy import displacy

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


def GetPunktSentenceTokenizer(file):
#    punkt_params = PunktParameters()
#    punkt_params.sent_starters = ['--', 'Cf']

    punkt_trainer = PunktTrainer()
    abbreviations = "v. Chr., Sarg., (?), Akk.Syll."
#    punkt_params = set(abbreviations)
   # punkt_sentence_trainer = PunktSentenceTokenizer()
    punkt_trainer.INCLUDE_ALL_COLLOCS = True
    punkt_trainer.INCLUDE_ABBREV_COLLOCS = True
    punkt_trainer.train(file, finalize=False, verbose=True)
#    punkt_trainer.freq_threshold()
    punkt_trainer.train(abbreviations, finalize=False, verbose=True)
    punkt_trainer.PUNCTUATION = (';', ':', ',', '.')
    punkt_trainer.sent_end_chars=('.')
    punkt_trainer.internal_punctuation=",:;?!"
#    punkt_trainer.sent_starters = ("--")
    params = punkt_trainer.get_params()
    params.INCLUDE_ABBREV_COLLOCS = True
    params.sent_starters = [' --', 'Cf']
    params.abbreviations = "v. Chr., Sarg., (?), Akk.Syll."
    params.sent_end_chars=['.']
    serialized = jsonpickle.encode(params)
#    print(json.dumps(json.loads(serialized), indent=2))
#    return PunktSentenceTokenizer(punkt_trainer.get_params())
    punkt_sentence_tokenizer = PunktSentenceTokenizer(params)
#    punkt_sentence_tokenizer.sent_starters("--")
    return punkt_sentence_tokenizer


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


SENTENCE_TYPES = { 'TITLE' : 'title', 'BIB_INFO' : 'bib_info', 'FURTHER_LITERATURE' : 'further_literature', 
                   'FURTHER_PUBLICATION' : 'further_publication', 'CONTENT_DESCRIPTION' : 'content_description', 
                   'YEAR_AND_PLACE' : 'year_and_place', 'COMMENT' : 'comment' }

def CreateClassifier():
    with open("./training/titles.txt") as titles:
       titles = [ (title, SENTENCE_TYPES['TITLE']) for title in titles ]
    with open("./training/comments.txt") as comments:
       comments = [ (comment, SENTENCE_TYPES['COMMENT']) for comment in comments ]
    with open("./training/bib_infos.txt") as bib_infos:
       bib_infos = [ (bib_info, SENTENCE_TYPES['BIB_INFO']) for bib_info in bib_infos ]
    with open("./training/years_and_places.txt") as years_and_places:
       years_and_places = [ (year_and_place, SENTENCE_TYPES['YEAR_AND_PLACE']) for year_and_place in years_and_places ]
    with open("./training/further_literatures.txt") as further_literatures:
       further_literatures = [ (further_literature, SENTENCE_TYPES['FURTHER_LITERATURE']) for further_literature in further_literatures ]
    with open("./training/further_publications.txt") as further_publications:
       further_publications = [ (further_publication, SENTENCE_TYPES['FURTHER_PUBLICATION']) for further_publication in further_publications ]
    with open("./training/content_descriptions.txt") as content_descriptions:
       content_descriptions = [ (content_description, SENTENCE_TYPES['CONTENT_DESCRIPTION']) for content_description in content_descriptions ]
    labeled = titles + comments + bib_infos + years_and_places + further_literatures + further_publications + content_descriptions
    SetupWordFeatures(labeled)
    random.shuffle(labeled)
    training = [(ExtractFeatures(example), sentence_type) for (example, sentence_type) in labeled]
    return nltk.NaiveBayesClassifier.train(training)


def Main():
    try:
         if len(sys.argv) != 2:
             print("Invalid arguments: usage " + sys.argv[0] + " hkl_extraction_file")
             sys.exit(-1)
         with open(sys.argv[1]) as f:
             #file = ReduceMultipleEmptyLinesToOne(f.read())
             file = FilterPageHeadings(f.read())
             entries = SplitToAuthorEntries(GetBufferLikeFile(file))
             #punkt_sentence_tokenizer = GetPunktSentenceTokenizer(file)
             #print(punkt_sentence_tokenizer.debug_decisions(file).format_debug_decision())
             #punkt_sentence_tokenizer.
             classifier = CreateClassifier()
             authors = []
             nlp = spacy.load('en_core_web_lg')
             #nlp = spacy.load('de_core_news_lg')
             for author, entry in entries:
                 print("---------------------------------------\nAUTHOR: " + author + '\n')
                 normalized_separations = re.sub(r'-\n', '', ''.join(entry))
                 normalized_newlines= re.sub(r'\n(?!\n)', ' ', normalized_separations)
                 author_tree = {}
                 author_tree['author'] = author
                 author_tree['titles'] = []
                 author_tree['bib_infos'] = []
                 author_tree['further_publications'] = []
                 author_tree['further_literatures'] = []
                 author_tree['content_descriptions'] = []
                 for to_parse in [ normalized_newlines ]:
                     print("XXX " + to_parse + '\n#########################################\n')
                     #sentences = punkt_sentence_tokenizer.tokenize(to_parse)
                     doc = nlp(to_parse)
                     #for sentence in sentences:
                     #options={"compact": True, "bg": "#09a3d5", "color": "red", "font": "Source Sans Pro"}
                     #displacy.serve(doc, style="dep", options=options)
                     for sentence in doc.sents:
                     #displacy.serve(doc, style = "ent")
                         sentence = sentence.orth_.strip()
                         print(sentence)
                         sentence_type = classifier.classify(ExtractFeatures(sentence))
                         if sentence_type == SENTENCE_TYPES['TITLE']:
                              title = sentence
                              author_tree['titles'].append({ 'title' :  title, 'bib_infos' : [], 'comments' : [], 'further_literatures' : [],
                                                             'further_publications' : []})
                         elif sentence_type == SENTENCE_TYPES['YEAR_AND_PLACE']:
                              if not author_tree['titles']:
                                 author_tree['titles'].append({ 'title': 'UNKNOWN TITLE 1', 'bib_infos' : [], 'comments' : [] })
                                 # raise Exception("Cannot insert year and place due to missing title")
                              author_tree['titles'][-1]['year_and_place'] = sentence
                         elif sentence_type == SENTENCE_TYPES['BIB_INFO']:
                             if not author_tree['titles']:
                                 #raise Exception("Cannot insert bib_info due to missing title for author " + author)
                                 author_tree['bib_infos'].append({'bib_info' : sentence })
                             else:
                                 author_tree['titles'][-1]['bib_infos'].append({'bib_info' : sentence, 'comments' : [] })
                         elif sentence_type == SENTENCE_TYPES['COMMENT']:
                             if not author_tree['titles']:
                                 author_tree['titles'].append({ 'title': 'UNKNOWN TITLE 2', 'bib_infos' : [], 'comments' : [] })
                                 #raise Exception("Cannot insert comment due to missing title for author " + author)
                             # We have a title comment if no bib_infos yet
                             if not author_tree['titles'][-1]['bib_infos']:
                                  author_tree['titles'][-1]['comments'].append({ 'comment' : sentence })
                             else:
                                 author_tree['titles'][-1]['bib_infos'][-1]['comments'].append( {'comment' : sentence })
                         elif sentence_type == SENTENCE_TYPES['FURTHER_LITERATURE']:
                             if not author_tree['titles']:
                                 author_tree['titles'].append({ 'title': 'UNKNOWN_TITLE 3', 'further_literatures' : []})
                             if not author_tree['titles'][-1]['further_literatures']:
                                 author_tree['titles'][-1]['further_literatures'].append({ 'further_literature' : sentence })
                         elif sentence_type == SENTENCE_TYPES['FURTHER_PUBLICATION']:
                             if not author_tree['titles']:
                                 author_tree['titles'].append({ 'title' : 'UNKOWN_TITLE 4', 'further_publications' : []})
                             if not author_tree['titles'][-1]['further_publications']:
                                 author_tree['titles'][-1]['further_publications'].append({ 'further_publication' : sentence })
                         elif sentence_type == SENTENCE_TYPES['CONTENT_DESCRIPTION']:
                             if not author_tree['titles']:
                                 author_tree['titles'].append({ 'title' : 'UNKOWN_TITLE 5', 'content_descriptions' : []})
                             if not author_tree['titles'][-1]['content_descriptions']:
                                 author_tree['titles'][-1]['content_descriptions'].append({ 'content_description' : sentence })
                 drop_falsey = lambda path, key, value: bool(value)
                 authors.append(remap(author_tree, visit=drop_falsey))
             #xml = dicttoxml(authors, custom_root='authors', attr_type=False)
             with open('./output.json', 'w') as json_out:
                 json.dump(authors, json_out)
    except Exception as e:
        print("ERROR: " + e)



if __name__ == "__main__":
    Main()
