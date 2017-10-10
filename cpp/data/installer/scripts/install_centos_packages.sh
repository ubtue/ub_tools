#!/bin/bash
yum -y install curl epel-release wget
cd /etc/yum.repos.d/
wget http://download.opensuse.org/repositories/security:shibboleth/CentOS_7/security:shibboleth.repo
yum -y update

yum -y install \
    ant clang composer gcc-c++.x86_64 git java-*-openjdk-devel make \
    httpd mariadb mod_ssl php php-devel php-gd php-intl php-ldap php-mbstring php-mcrypt php-mysql php-xsl \
    boost-devel ca-certificates curl-openssl file-devel kyotocabinet-devel leptonica libarchive-devel libcurl-openssl-devel.x86_64 libuuid-devel libwebp libxml2-devel.x86_64 lsof lz4 mariadb-devel.x86_64 mawk openjpeg-libs openssl-devel pcre-devel policycoreutils-python poppler poppler-utils tokyocabinet-devel unzip \
    tesseract tesseract-langpack-bul tesseract-langpack-ces tesseract-langpack-dan tesseract-langpack-deu tesseract-langpack-fin tesseract-langpack-fra tesseract-langpack-grc tesseract-langpack-heb tesseract-langpack-hun tesseract-langpack-ita tesseract-langpack-lat tesseract-langpack-nld tesseract-langpack-nor tesseract-langpack-pol tesseract-langpack-por tesseract-langpack-rus tesseract-langpack-slv tesseract-langpack-spa tesseract-langpack-swe

# in centos, there is no "tesseract-langpack-eng", it seems to be part of the default installation
