#!/bin/bash

function expect_success_cpp() {
    output=$(./bib_ref_parser_test "$@")
    if [ $? != 0 ]; then
	echo "C++ test failed, expected success:" "$@"
	exit 1
    fi
    if [[ $output != $2 ]]; then
        echo "Parser emitted $output but we expected $2\!"
        exit 2
    fi
}

function expect_failure_cpp() {
    ./bib_ref_parser_test "$@"
    if [ $? == 0 ]; then
	echo "C++ test failed, expected failure:" "$@"
	exit 1
    fi
}

expect_success_cpp "22" "01022000:01022999"         # A single chapter.
expect_success_cpp "1,2" "01001002:01001002"        # A single chapter with a single verse.
expect_success_cpp "1,  2" "01001002:01001002"      # A single chapter with a single verse and embedded spaces.
expect_success_cpp "1,2b" "01001002:01001002"       # A single chapter with a single verse.
expect_success_cpp "1-3" "01001000:01003999"        # A chapter range w/o verses.
expect_failure_cpp "3-1" "01003999:01001000"        # An inverted chapter range w/o verses.
expect_success_cpp "1,2-3" "01001002:01001003"      # One chapter, verse range.
expect_success_cpp "1,2a-3" "01001002:01001003"     # One chapter, verse range.
expect_failure_cpp "1,3-2" "01001003:01001002"      # One chapter, inverted verse range.
expect_success_cpp "1,2-5,17" "01001002:01005017"   # chapter1/verse1 - chapter2/verse2.
expect_success_cpp "1,2d-5,17" "01001002:01005017"  # chapter1/verse1 - chapter2/verse2.
expect_success_cpp "1,2-5,17c" "01001002:01005017"  # chapter1/verse1 - chapter2/verse2.
expect_success_cpp "1,2b-5,17c" "01001002:01005017" # chapter1/verse1 - chapter2/verse2.
expect_failure_cpp "6,2-5,17" "01006002:01005017"   # chapter1/verse1 - chapter2/verse2 but chapter2 < chapter1.
expect_success_cpp "17,3.6" "01017093:01017003" "01017006:01017006" # A single chapter and two nonconsecutive single verses.
expect_failure_cpp "17,6.3" "01017006:01017006" "01017003:01017003" # A single chapter and two nonconsecutive single verses.
expect_failure_cpp "17,3-6.4-7" "01017006:01017006" "01017003:01017003" # A single chapter and two overlapping verse ranges.
expect_success_cpp "17,3.6-8" "01017003:01017003" "01017006:01017008" # A verse and a verse range.
expect_failure_cpp "17,3.8-6" "01017003:01017003" "01017008:01017006" # A verse and a verse range.
expect_success_cpp "2,1-3.5" "01002001:01002003" "01002005:01002005" # A verse range followed by a single verse.
expect_success_cpp "2,1-3.5-7" "01002001:01002003" "01002005:01002007" # Two separate verse ranges.
expect_success_cpp "1,2a-3b" "01001002:01001003" # A single chapter and two partial verses.
