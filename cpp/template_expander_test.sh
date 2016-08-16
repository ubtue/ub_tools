#!/bin/bash

# Tests for expand_template_test.
set -o errexit -o nounset

# Test 0
./expand_template_test test_input/test0.template x:y > /tmp/test.out
diff --brief /tmp/test.out test_input/test0.template.out
rm -f /tmp/test.out

# Test 1
./expand_template_test test_input/test1.template fred:Joe text:"Hi there!" > /tmp/test.out
diff --brief /tmp/test.out test_input/test1.template.out
rm -f /tmp/test.out

# Test 2
./expand_template_test test_input/test2.template item:fish:cheese price:3.55:2.99 > /tmp/test.out
diff --brief /tmp/test.out test_input/test2.template.out
rm -f /tmp/test.out

# Test 3
./expand_template_test test_input/test3.template var:x > /tmp/test.out
diff --brief /tmp/test.out test_input/test3.template.out
rm -f /tmp/test.out
