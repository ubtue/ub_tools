#!/bin/bash
# Test script used w/ exec_test.

trap 'echo Exiting; exit -1' SIGTERM

while :; do
    sleep 1
    echo Looping
done
