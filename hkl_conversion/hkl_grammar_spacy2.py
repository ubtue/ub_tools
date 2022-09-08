#!/usr/bin/python3

import re
import regex
import sys
import re, pprint
import random
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
from spacy.tokens import Doc,DocBin

#from stanza.pipeline.processor import ProcessorVariant, register_processor_variant, Processor, register_processor
#from spacy.pipeline.senter import DEFAULT_SENTER_MODEL


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
      #stanza.download("de")
      #nlp = spacy_stanza.load_pipeline("de", processors="tokenize,pos,mwt,lemma,depparse,ner")
      #nlp = spacy.load("de_core_news_lg")
      #nlp.remove_pipe("senter");
      #config = {"model": DEFAULT_SENTER_MODEL,}
      #nlp.add_pipe("senter", config=config)
      #nlp = spacy.load("de_dep_news_trf")
      #nlp = spacy.load("xx_sent_ud_sm")
      #nlp = spacy.load("en_core_web_md", exclude=["ner", "senter", "parser", "tagger", "attribute_ruler", "lemmatizer"])
      nlp = spacy.load("ner_training/training/output/model-best")
      nlp.add_pipe("senter", source=spacy.load("senter_training/training/output/model-best"))
      #nlp = spacy.load("senter_training/training/output/model-best")
      nlp.tokenizer.add_special_case("Zweispr.", [{"ORTH" : "Zweispr."}])
      nlp.tokenizer.add_special_case("Sumer.", [{"ORTH" : "Sumer."}])
      nlp.tokenizer.add_special_case("assyr.", [{"ORTH" : "assyr."}])
      nlp.tokenizer.add_special_case("Neuassyr.", [{"ORTH" : "Neuassyr."}])
      nlp.tokenizer.add_special_case("Vgl.", [{"ORTH" : "Vgl."}])
      nlp.tokenizer.add_special_case("Sarg.", [{"ORTH" : "Sarg."}])
      nlp.tokenizer.add_special_case("Z. 1)", [{"ORTH" : "Z. 1)"}])
      nlp.tokenizer.add_special_case("2. Auflage", [{"ORTH" : "2. Auflage"}])
      prefixes = nlp.Defaults.prefixes + [r"\-\-|Cf",]
      prefix_regex = spacy.util.compile_prefix_regex(prefixes)
      #nlp.senter.prefix_search = prefix_regex.search
      #matcher = PhraseMatcher(nlp.vocab)
      #terms = ["Zweispr. Kultlied", "Sumer. Kultlied.", "Zweispr. Hymnus.", "vol. II."]
      #patterns = [nlp.make_doc(text) for text in terms]
      #matcher.add("TerminologyList", patterns)
      #nlp.add_pipe("entity_ruler", before="ner")
      analysis = nlp.analyze_pipes(pretty=True)
      nlp.to_disk("./spacy_pipeline_xx_sent_ud")
      docs = []
      for author, entry in entries:
          normalized_separations = re.sub(r'-\n', '', ''.join(entry))
          normalized_newlines= re.sub(r'\n(?!\n)', ' ', normalized_separations)
          sentences = []
          sentence_groups = []
          print("AUTHOR: " + author)
          to_parse = normalized_newlines
          print("ORIG: " + to_parse)
          doc = nlp(to_parse)
          docs.append(doc)
          for ent in doc.ents:
              print("\n*********************************************\n")
              print(ent)
          #displacy.serve(doc, style="ent")
          print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
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
      #displacy.serve(doc.from_docs(docs), style="ent")
      return authors_and_sentence_groups


def PrintResultsRaw(authors_and_sentence_groups):
    for author, sentence_groups in authors_and_sentence_groups:
        print("---------------------------------------\nAUTHOR: " + author + '\n')
        for sentences in sentence_groups:
             print("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%")
             for sentence in sentences:
                 print("SENT: " + sentence)

def HandleMissingTitle(one_author_tree):
    one_author_tree['titles'].append( { 'title' : 'MISATTRIBUTED TITLE (!!)',  'elements' : []  } )
    return one_author_tree


def GetHighestLabel(cats):
    return list(sorted(cats, key=cats.get, reverse=True))[0]


def ClassifySentences(authors_and_sentence_groups, output):
    #classifier = nltk_classification.CreateClassifier()
    classifier = spacy.load("training/output/model-best")
    all_authors = []
    one_author_tree = dict()

    for author, sentence_groups in authors_and_sentence_groups:
        print("---------------------------------------\nAUTHOR: " + author + '\n')
        one_author_tree['author'] = author
        one_author_tree['titles'] = []
        for sentences in sentence_groups:
             print("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%")
             for sentence in sentences:
                 sentence_type = GetHighestLabel(classifier(sentence).cats).upper()
                 print("SENTENCE TYPE: " + sentence_type)
                 if sentence_type == 'TITLE':
                      title = sentence
                      one_author_tree['titles'].append({ 'title' : title, 'elements' : [] })
                 elif sentence_type == 'YEAR_AND_PLACE':
                      if not one_author_tree['titles']:
                          one_author_tree = HandleMissingTitle(one_author_tree)
                      one_author_tree['titles'][-1]['elements'].append( {'year_and_place' :  sentence })
                 elif sentence_type == 'BIB_INFO':
                     if not one_author_tree['titles']:
                         one_author_tree = HandleMissingTitle(one_author_tree)
                     one_author_tree['titles'][-1]['elements'].append( { 'bib_info' : sentence })
                 elif sentence_type == 'COMMENT':
                     if not one_author_tree['titles']:
                         one_author_tree = HandleMissingTitle(one_author_tree)
                     one_author_tree['titles'][-1]['elements'].append({ 'comment' : sentence})
                 elif sentence_type == 'INTERNAL_REFERENCE':
                     if not one_author_tree['titles']:
                         one_author_tree = HandleMissingTitle(one_author_tree)
                     one_author_tree['titles'][-1]['elements'].append({ 'internal_reference' : sentence })

        drop_falsey = lambda path, key, value: bool(value)
        all_authors.append(remap(one_author_tree, visit=drop_falsey))
    with open(output, 'w') as json_out:
        json.dump(all_authors, json_out)
    return all_authors


def WriteOutNER(docs, nlp):
     doc_bin = DocBin()
     doc_bin.add(Doc(nlp.vocab).from_docs(docs))
     doc_bin.to_disk("./ner_test_docs1.spacy")


def LabelEntities(authors_with_classifications):
     ner_nlp = spacy.load("ner_training/training/output/model-best")
     plain_nlp = spacy.load("ner_training/training/output/model-best", exclude=["ner"], vocab=ner_nlp.vocab)
     docs = []
     for author_and_titles in authors_with_classifications:
         docs.append(plain_nlp(author_and_titles['author']))
         for title_and_elements in author_and_titles['titles']:
             docs.append(plain_nlp(title_and_elements['title']))
             if 'elements' in title_and_elements:
                 for element in title_and_elements['elements']:
                     print(element)
                     for element_type, element_content in element.items():
                         if element_type == 'internal_reference' or element_type == 'bib_info':
                             doc = ner_nlp(element_content)
                             docs.append(doc)
                         else:
                             docs.append(plain_nlp(element_content))
     WriteOutNER(docs, ner_nlp)
     #displacy.serve(doc.from_docs(docs), style="ent")


def Main():
    try:
         if len(sys.argv) != 3:
             print("Invalid arguments: usage " + sys.argv[0] + " hkl_extraction_file output.json")
             sys.exit(-1)
         with open(sys.argv[1]) as f:
             file = FilterPageHeadings(f.read())
             entries = SplitToAuthorEntries(GetBufferLikeFile(file))
             authors_and_sentence_groups = SplitToAuthorSentenceGroupsAndSentences(entries)
             authors_with_classifications = ClassifySentences(authors_and_sentence_groups, sys.argv[2])
             LabelEntities(authors_with_classifications)


    except Exception as e:
        print("ERROR: " + e)



if __name__ == "__main__":
    Main()
