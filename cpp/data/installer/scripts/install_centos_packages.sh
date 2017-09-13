#!/bin/bash
yum update
yum -y install epel-release
yum -y install mawk git mariadb mariadb-server httpd php php-devel php-mcrypt php-intl php-ldap php-mysql php-xsl php-gd php-mbstring php-mcrypt java-*-openjdk-devel mawk mod_ssl epel-release wget policycoreutils-python

cd /etc/yum.repos.d/
wget http://download.opensuse.org/repositories/security:shibboleth/CentOS_7/security:shibboleth.repo
yum -y install curl-openssl mutt golang lsof
yum -y install clang gcc-c++.x86_64 file-devel pcre-devel openssl-devel kyotocabinet-devel tokyocabinet-devel poppler-utils libwebp mariadb-devel.x86_64 libxml2-devel.x86_64 libcurl-openssl-devel.x86_64 ant lz4 unzip libarchive-devel boost-devel libuuid-devel

yum -y install /mnt/ZE020150/IT-Abteilung/02_Projekte/11_KrimDok_neu/05_Pakete/*.rpm
ln -s /usr/share/tessdata/deu.traineddata /usr/share/tesseract/tessdata/deu.traineddata
