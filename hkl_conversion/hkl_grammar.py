#!/usr/bin/python3

import re
import regex
import sys
from lark import Lark, Transformer

hkl_parser = Lark(r"""
    ?author_bibliography: sentence+ NL*
    non_empty_string: /[^\t\s\n\r,]+/
    sentence.1: non_empty_string ((IWS|NL|COW) non_empty_string)* END_OF_SENTENCE
    number: INT
    IWS : /[\s\t]+/
    NL : /\n/
    END_OF_SENTENCE: "." NL
    EMPTY_LINE_SEPARATOR: "\n\n"
    COW : IWS* "," IWS* // Comma with optional whitespace


//    %import common._STRING_INNER
    %import common.INT
//    %import common.WS
    """, start='author_bibliography', ambiguity="explicit", debug=True)



def SplitToAuthorEntries(file):
    entry = []
    entries = []
    author = ''
    author_match_regex = regex.compile(r'^\p{Lu}\p{Ll}+\s*,(\s+\p{Lu}([.]|\p{Ll})+)+$')
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
             for author, entry in entries:
                 print("---------------------------------------\nAUTHOR: " + author + '\n')
                 print(entry)
                 #print("FOR AUTHOR: " + entry[0].rstrip())
                 #hkl_parser.parse(''.join(entry[1:]))


#             print(hkl_parser.parse(f.read()).pretty())
    except Exception as e:
        print("ERROR: " + e)



if __name__ == "__main__":
    Main()
