#!/bin/bash
set -o nounset

./files_differ_test test_input/empty1 test_input/empty1
if [ $? ]; then
    echo "Test failed, two empty files differ!"
    exit 1
fi

./files_differ_test test_input/multiple_8k_blocks test_input/empty1
if [ ! $? ]; then
    echo "Test failed, comparsion of a non-empty against an empty file failed!"
    exit 2
fi

./files_differ_test test_input/empty1 test_input/multiple_8k_blocks
if [ ! $? ]; then
    echo "Test failed, comparsion of an empty against a non-empty file failed!"
    exit 3
fi

./files_differ_test test_input/multiple_8k_blocks test_input/multiple_8k_blocks
if [ $? ]; then
    echo "Test failed, two identical non-empty files differ!"
    exit 1
fi
