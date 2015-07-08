#!/bin/bash

# The following are necessary to build the C++ programs on Centos 7:
yum install file-devel # for libmagic
yum install pcre-devel # for libpcre
yum install openssl-devel # for libcrypto
yum install kyotocabinet-devel # for libkyotocabinet
