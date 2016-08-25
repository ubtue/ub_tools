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

# Test 4
./expand_template_test test_input/test4.template firstname:Johannes lastname:Ruscheinski url:http1:http2 \
                       title:T1:T1000 > /tmp/test.out
diff --brief /tmp/test.out test_input/test4.template.out
rm -f /tmp/test.out

# Test 5
./expand_template_test test_input/test5.template var:1 > /tmp/test.out
diff --brief /tmp/test.out test_input/test5.template.out
rm -f /tmp/test.out

# Test 6
./expand_template_test test_input/test6.template x:y > /tmp/test.out
diff --brief /tmp/test.out test_input/test6.template.out
rm -f /tmp/test.out

# Test 7
./expand_template_test test_input/test7.template x:"Bob" y:"Bob" > /tmp/test.out
diff --brief /tmp/test.out test_input/test7.template.out
rm -f /tmp/test.out

# Test 8
./expand_template_test test_input/test8.template x:"Bob" y:"Bill" > /tmp/test.out
diff --brief /tmp/test.out test_input/test8.template.out
rm -f /tmp/test.out

# Test 9
./expand_template_test test_input/test9.template x:"Bob" > /tmp/test.out
diff --brief /tmp/test.out test_input/test9.template.out
rm -f /tmp/test.out

# Test 10
./expand_template_test test_input/test10.template x:"Bob" > /tmp/test.out
diff --brief /tmp/test.out test_input/test10.template.out
rm -f /tmp/test.out

