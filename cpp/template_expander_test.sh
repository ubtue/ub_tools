#!/bin/bash

# Tests for expand_template_test.
set -o errexit -o nounset

echo "Test 0"
./expand_template_test test_input/test0.template x:y > /tmp/test.out
diff --brief /tmp/test.out test_input/test0.template.out
rm -f /tmp/test.out

echo "Test 1"
./expand_template_test test_input/test1.template fred:Joe text:"Hi there!" > /tmp/test.out
diff --brief /tmp/test.out test_input/test1.template.out
rm -f /tmp/test.out

echo "Test 2"
./expand_template_test test_input/test2.template item:fish:cheese price:3.55:2.99 > /tmp/test.out
diff --brief /tmp/test.out test_input/test2.template.out
rm -f /tmp/test.out

echo "Test 3"
./expand_template_test test_input/test3.template var:x > /tmp/test.out
diff --brief /tmp/test.out test_input/test3.template.out
rm -f /tmp/test.out

echo "Test 4"
./expand_template_test test_input/test4.template firstname:Johannes lastname:Ruscheinski url:http1:http2 \
                       title:T1:T1000 > /tmp/test.out
diff --brief /tmp/test.out test_input/test4.template.out
rm -f /tmp/test.out

echo "Test 5"
./expand_template_test test_input/test5.template var:1 > /tmp/test.out
diff --brief /tmp/test.out test_input/test5.template.out
rm -f /tmp/test.out

echo "Test 6"
./expand_template_test test_input/test6.template x:y > /tmp/test.out
diff --brief /tmp/test.out test_input/test6.template.out
rm -f /tmp/test.out

echo "Test 7"
./expand_template_test test_input/test7.template x:"Bob" y:"Bob" > /tmp/test.out
diff --brief /tmp/test.out test_input/test7.template.out
rm -f /tmp/test.out

echo "Test 8"
./expand_template_test test_input/test8.template x:"Bob" y:"Bill" > /tmp/test.out
diff --brief /tmp/test.out test_input/test8.template.out
rm -f /tmp/test.out

echo "Test 9"
./expand_template_test test_input/test9.template x:"Bob" > /tmp/test.out
diff --brief /tmp/test.out test_input/test9.template.out
rm -f /tmp/test.out

echo "Test 10"
./expand_template_test test_input/test10.template x:"Bob" > /tmp/test.out
diff --brief /tmp/test.out test_input/test10.template.out
rm -f /tmp/test.out

echo "Test 11a"
./expand_template_test test_input/test11.template x:"Bob" y:"Bill" > /tmp/test.out
diff --brief /tmp/test.out test_input/test11a.template.out
rm -f /tmp/test.out

echo "Test 11b"
./expand_template_test test_input/test11.template x:"Bob" y:"Joe" > /tmp/test.out
diff --brief /tmp/test.out test_input/test11b.template.out
rm -f /tmp/test.out

echo "Test 12a"
./expand_template_test test_input/test12.template x:"Bob" y:"Joe" > /tmp/test.out
diff --brief /tmp/test.out test_input/test12a.template.out
rm -f /tmp/test.out

echo "Test 12b"
./expand_template_test test_input/test12.template x:"Fred" y:"Bill" > /tmp/test.out
diff --brief /tmp/test.out test_input/test12b.template.out
rm -f /tmp/test.out

echo "Test 12c"
./expand_template_test test_input/test12.template x:"Fred" y:"Joe" > /tmp/test.out
diff --brief /tmp/test.out test_input/test12c.template.out
rm -f /tmp/test.out

echo "*** Passed all tests ***"
