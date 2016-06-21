#!/bin/bash

# Tests for expand_template_test.
set -o errexit -o nounset


./expand_template_test test_input/test1.template fred:Joe text:"Hi there!" > /tmp/test.out
diff --brief /tmp/test.out test_input/test1.template.out

rm -f /tmp/test.out
./expand_template_test test_input/test2.template item:fish:cheese price:3.55:2.99 > /tmp/test.out
diff --brief /tmp/test.out test_input/test2.template.out

rm -f /tmp/test.out
