#!/bin/bash

function expect_success_cpp() {
    ./bib_ref_parser_test "$@"
    if [ $? != 0 ]; then
	echo "C++ test failed, expected success:" "$@"
	exit 1
    fi
}

function expect_failure_cpp() {
    ./bib_ref_parser_test "$@"
    if [ $? == 0 ]; then
	echo "C++ test failed, expected failure:" "$@"
	exit 1
    fi
}

function expect_success_js() {
    nodejs ./bib_ref_parser_test.js "$@"
    if [ $? != 0 ]; then
	echo "JavaScript test failed, expected success:" "$@"
	exit 1
    fi
}

function expect_failure_js() {
    nodejs ./bib_ref_parser_test.js "$@"
    if [ $? == 0 ]; then
	echo "JavaScript test failed, expected failure:" "$@"
	exit 1
    fi
}

expect_success_cpp "22" "0102200:0102200"         # A single chapter.
expect_success_cpp "1,2" "0100102:0100102"        # A single chapter with a single verse.
expect_success_cpp "1,2b" "0100102:0100102"       # A single chapter with a single verse.
expect_success_cpp "1-3" "0100100:0100300"        # A chapter range w/o verses.
expect_failure_cpp "3-1" "0100300:0100100"        # An inverted chapter range w/o verses.
expect_success_cpp "1,2-3" "0100102:0100103"      # One chapter, verse range.
expect_success_cpp "1,2a-3" "0100102:0100103"     # One chapter, verse range.
expect_failure_cpp "1,3-2" "0100103:0100102"      # One chapter, inverted verse range.
expect_success_cpp "1,2-5,17" "0100102:0100517"   # chapter1/verse1 - chapter2/verse2.
expect_success_cpp "1,2d-5,17" "0100102:0100517"  # chapter1/verse1 - chapter2/verse2.
expect_success_cpp "1,2-5,17c" "0100102:0100517"  # chapter1/verse1 - chapter2/verse2.
expect_success_cpp "1,2b-5,17c" "0100102:0100517" # chapter1/verse1 - chapter2/verse2.
expect_failure_cpp "6,2-5,17" "0100602:0100517"   # chapter1/verse1 - chapter2/verse2 but chapter2 < chapter1.
expect_success_cpp "17,3.6" "0101703:0101703" "0101706:0101706" # A single chapter and two nonconsecutive single verses.
expect_success_cpp "17,6.3" "0101706:0101706" "0101703:0101703" # A single chapter and two nonconsecutive single verses.
expect_success_cpp "17,3.6-8" "0101703:0101703" "0101706:0101708" # A verse and a verse range.
expect_failure_cpp "17,3.8-6" "0101703:0101703" "0101708:0101706" # A verse and a verse range.
expect_success_cpp "2,1-3.5" "0100201:0100203" "0100205:0100205" # A verse range followed by a single verse.
expect_success_cpp "2,1-3.5-7" "0100201:0100203" "0100205:0100207" # Two separate verse ranges.


expect_success_js "22" "0102200:0102200"         # A single chapter.
expect_success_js "1,2" "0100102:0100102"        # A single chapter with a single verse.
expect_success_js "1,2b" "0100102:0100102"       # A single chapter with a single verse.
expect_success_js "1-3" "0100100:0100300"        # A chapter range w/o verses.
expect_failure_js "3-1" "0100300:0100100"        # An inverted chapter range w/o verses.
expect_success_js "1,2-3" "0100102:0100103"      # One chapter, verse range.
expect_success_js "1,2a-3" "0100102:0100103"     # One chapter, verse range.
expect_failure_js "1,3-2" "0100103:0100102"      # One chapter, inverted verse range.
expect_success_js "1,2-5,17" "0100102:0100517"   # chapter1/verse1 - chapter2/verse2.
expect_success_js "1,2d-5,17" "0100102:0100517"  # chapter1/verse1 - chapter2/verse2.
expect_success_js "1,2-5,17c" "0100102:0100517"  # chapter1/verse1 - chapter2/verse2.
expect_success_js "1,2b-5,17c" "0100102:0100517" # chapter1/verse1 - chapter2/verse2.
expect_failure_js "6,2-5,17" "0100602:0100517"   # chapter1/verse1 - chapter2/verse2 but chapter2 < chapter1.
expect_success_js "17,3.6" "0101703:0101703" "0101706:0101706" # A single chapter and two nonconsecutive single verses.
expect_success_js "17,6.3" "0101706:0101706" "0101703:0101703" # A single chapter and two nonconsecutive single verses.
expect_success_js "17,3.6-8" "0101703:0101703" "0101706:0101708" # A verse and a verse range.
expect_failure_js "17,3.8-6" "0101703:0101703" "0101708:0101706" # A verse and a verse range.
expect_success_js "2,1-3.5" "0100201:0100203" "0100205:0100205" # A verse range followed by a single verse.
expect_success_js "2,1-3.5-7" "0100201:0100203" "0100205:0100207" # Two separate verse ranges.
