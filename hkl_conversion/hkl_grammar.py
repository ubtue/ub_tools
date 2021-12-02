#!/usr/bin/python3

import re
import regex
import sys
from lark import Lark, Transformer

hkl_parser = Lark(r"""
//    ?author_bibliography: bibliographic_item+ NL*
//    ?bibliographic_item: full_citation_with_short_title
    ?bibliographic_item: full_citation_with_short_title
    full_citation_with_short_title: short_title /\s*=\s*/ full_title place " " year END_OF_SENTENCE note
    short_title: WORD (/\s+/ WORD)*
    full_title: sentence
//    place: WORD (/\s+/ WORD)*
    place: WORD
    note: (WORD|NUMBER) (IWS (WORD|NUMBER))* | sentence* (IWS sentence)*
    year: /[12]\d{3}/
    some_other_stuff: /[A-Z][a-z].*/
    non_empty_string: /[^\t\s\n\r,]+/
    //sentence: non_empty_string ((IWS|NL|COW) non_empty_string)* END_OF_SENTENCE
    sentence: (WORD|NUMBER|BRACKET_EXPRESSION) ((IWS|NL|COW|"=") (WORD|NUMBER|BRACKET_EXPRESSION))* END_OF_SENTENCE
    BRACKET_EXPRESSION: /[(].*[)]/
    number: INT
    IWS : /[\s\t]+/
    NL : /\n/
    END_OF_SENTENCE: "." (IWS|NL)*
    EMPTY_LINE_SEPARATOR: "\n\n"
    COW : IWS* "," IWS* // Comma with optional whitespace
    WORD: /\w+/


//    %import common._STRING_INNER
    %import common.INT
//    %import common.WORD
    %import common.NUMBER
//    %import common.WS
    """, start='bibliographic_item', ambiguity="explicit")



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

def Main():
    try:
         with open(sys.argv[1]) as f:
             file = ReduceMultipleEmptyLinesToOne(f.read())
             file = FilterPageHeadings(file)
             #print(file)
             entries = SplitToAuthorEntries(GetBufferLikeFile(file))
#             for author, entry in entries[:4]:
             for author, entry in [entries[0]]:
                 print("---------------------------------------\nAUTHOR: " + author + '\n')
                 normalized_separations = re.sub(r'-\n', '', ''.join(entry))
                 normalized_newlines= re.sub(r'\n(?!\n)', ' ', normalized_separations)
                 print(normalized_newlines)
                 print("#################################")
                 #print(normalized_newlines.split('\n')[0])
                 to_parse = "Assyrian manual = An Assyrian manual for the use of beginners in the study of the Assyrian language. Chicago 1886. 2. Auflage Chicago 1892 (mir nicht zugänglich). Nicht berücksichtigt."
                 print(to_parse + '\n#########################################\n')
                 #hkl_parser.parse(normalized_newlines.split('\n')[0] + '\n')
                 print(hkl_parser.parse(to_parse).pretty())
    except Exception as e:
        print("ERROR: " + e)



if __name__ == "__main__":
    Main()
