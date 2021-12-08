#!/usr/bin/python3

import re
import regex
import sys
import nltk, re, pprint
#nltk.download('punkt')
#nltk.download('averaged_perceptron_tagger')
from nltk.corpus import state_union
from nltk.tokenize.punkt import PunktSentenceTokenizer, PunktTrainer
nltk.download('stopwords')
import random
from lark import Lark, Transformer
from langdetect import detect
from dicttoxml import dicttoxml
import json
from boltons.iterutils import remap

hkl_parser = Lark(r"""
//    ?author_bibliography: bibliographic_item+ NL*
//    ?bibliographic_item: full_citation_with_short_title
//    ?bibliographic_item: full_citation_with_short_title
//    full_citation_with_short_title: short_title /\s*=\s*/ full_title place " " year END_OF_SENTENCE note
    ?bibliographic_item: bibliographic_reference | place_and_year | title
//    title.4: /\s*[^\s]+(\s+[^\s]+)*/
    title.4: (WORD|NUMBER|BRACKET_EXPRESSION) ((IWS|NL|COW|"=") (WORD|NUMBER|BRACKET_EXPRESSION))*
    short_title: WORD (/\s+/ WORD)*
    full_title: sentence
    //place: WORD (/\s+/ WORD)*
    bibliographic_reference.0: abbrev /\s+/ number
    abbrev: /[A-Z]+/
    place_and_year.2: place /\s+/ year
    place: WORD
    note.3: (WORD|NUMBER) (IWS (WORD|NUMBER))* | sentence* (IWS sentence)*
    year: /[12]\d{3}/
    some_other_stuff: /[A-Z][a-z].*/
    non_empty_string: /[^\t\s\n\r,]+/
    //sentence: non_empty_string ((IWS|NL|COW) non_empty_string)* END_OF_SENTENCE
    sentence: (WORD|NUMBER|BRACKET_EXPRESSION) ((IWS|NL|COW|"=") (WORD|NUMBER|BRACKET_EXPRESSION))* END_OF_SENTENCE
    BRACKET_EXPRESSION: /[(].*[)]/
    number: INT
    IWS : /[\s\t]+/
    NL : /\n/
    END_OF_SENTENCE: /[.]/
    EMPTY_LINE_SEPARATOR: "\n\n"
    COW : IWS* "," IWS* // Comma with optional whitespace
    WORD: /[\w']+/


//    %import common._STRING_INNER
    %import common.INT
//    %import common.WORD
    %import common.NUMBER
//    %import common.WS
    """, start='bibliographic_item', parser='earley')



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
    return re.sub(r'(?:^\d+\n\n+[^,]+\n)|(?:^[^,]+\n\n+\d+\n)', '\n', file, flags=re.MULTILINE)


def GetPunktSentenceTokenizer(file):
    punkt_trainer = PunktTrainer()
    abbreviations = "v. Chr., Sarg."
    punkt_params = set(abbreviations)
    punkt_sentence_trainer = PunktSentenceTokenizer()
    punkt_trainer.train(file, finalize=False, verbose=False)
    punkt_trainer.train(abbreviations, finalize=False, verbose=False)
    return PunktSentenceTokenizer(punkt_trainer.get_params())


def ContainsProbableEditionYear(sentence):
    return bool(re.search(r'\s+[12][0789]\d{2}\b', sentence))


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


def Main():
    try:
         if len(sys.argv) != 2:
             print("Invalid arguments: usage " + sys.argv[0] + " hkl_extraction_file")
             sys.exit(-1)
         with open(sys.argv[1]) as f:
             file = ReduceMultipleEmptyLinesToOne(f.read())
             file = FilterPageHeadings(file)
             entries = SplitToAuthorEntries(GetBufferLikeFile(file))
             punkt_sentence_tokenizer = GetPunktSentenceTokenizer(file)
             classifier = CreateClassifier()
             authors = []
             for author, entry in entries:
#                 print("---------------------------------------\nAUTHOR: " + author + '\n')
                 normalized_separations = re.sub(r'-\n', '', ''.join(entry))
                 normalized_newlines= re.sub(r'\n(?!\n)', ' ', normalized_separations)
                 author_tree = {}
                 author_tree['author'] = author
                 author_tree['titles'] = []
                 author_tree['bib_infos'] = []
                 print("#################################")
                 for to_parse in normalized_newlines.split('\n'):
                     print(to_parse + '\n#########################################\n')
                     sentences = punkt_sentence_tokenizer.tokenize(to_parse)
                     for sentence in sentences:
                         print(sentence + " XXX " + classifier.classify(ExtractFeatures(sentence)))
                         sentence_type = classifier.classify(ExtractFeatures(sentence))
                         if sentence_type == SENTENCE_TYPES['TITLE']:
                              title = sentence
                              author_tree['titles'].append({ 'title' :  title, 'bib_infos' : [], 'comments' : [] })
                         elif sentence_type == SENTENCE_TYPES['YEAR_AND_PLACE']:
                              if not author_tree['titles']:
                                 raise Exception("Cannot insert year and place due to missing title")
                              author_tree['titles'][-1]['year_and_place'] = sentence
                         elif sentence_type == SENTENCE_TYPES['BIB_INFO']:
                             if not author_tree['titles']:
                                 #raise Exception("Cannot insert bib_info due to missing title for author " + author)
                                 author_tree['bib_infos'].append({'bib_info' : sentence })
                             else:
                                 author_tree['titles'][-1]['bib_infos'].append({'bib_info' : sentence, 'comments' : [] })
                         elif sentence_type == SENTENCE_TYPES['COMMENT']:
                             if not author_tree['titles']:
                                 raise Exception("Cannot insert comment due to missing title for author " + author)
                             # We have a title comment if no bib_infos yet
                             if not author_tree['titles'][-1]['bib_infos']:
                                  author_tree['titles'][-1]['comments'].append({ 'comment' : sentence })
                             else:
                                 author_tree['titles'][-1]['bib_infos'][-1]['comments'].append( {'comment' : sentence })
                 drop_falsey = lambda path, key, value: bool(value)
                 authors.append(remap(author_tree, visit=drop_falsey))
             #xml = dicttoxml(authors, custom_root='authors', attr_type=False)
             with open('./output.json', 'w') as json_out:
                 json.dump(authors, json_out)
    except Exception as e:
        print("ERROR: " + e)



if __name__ == "__main__":
    Main()
