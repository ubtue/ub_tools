#!/bin/bash
yum update
yum -y install epel-release
yum -y install mawk git mariadb httpd php php-devel php-mcrypt php-intl php-ldap php-mysql php-xsl php-gd php-mbstring php-mcrypt java-*-openjdk-devel mawk mod_ssl epel-release wget policycoreutils-python

cd /etc/yum.repos.d/
wget http://download.opensuse.org/repositories/security:shibboleth/CentOS_7/security:shibboleth.repo
yum -y install curl-openssl mutt golang lsof
yum -y install clang gcc-c++.x86_64 file-devel pcre-devel openssl-devel kyotocabinet-devel tokyocabinet-devel poppler-utils libwebp mariadb-devel.x86_64 libxml2-devel.x86_64 libcurl-openssl-devel.x86_64 ant lz4 unzip libarchive-devel boost-devel libuuid-devel
yum -y install ca-certificates leptonica libwebp openjpeg-libs poppler poppler-utils

# install tesseract. in centos, there is no language pack for "eng", it seems to be part of the default installation
yum -y install tesseract tesseract-langpack-bul tesseract-langpack-ces tesseract-langpack-dan tesseract-langpack-deu tesseract-langpack-fin tesseract-langpack-fra tesseract-langpack-hun tesseract-langpack-ita tesseract-langpack-nld tesseract-langpack-nor tesseract-langpack-pol tesseract-langpack-por tesseract-langpack-rus tesseract-langpack-slv tesseract-langpack-swe
