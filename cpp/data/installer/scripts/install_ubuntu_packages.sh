#!/bin/bash
apt-get --yes update

# install software-properties-common for apt-add-repository
apt-get --yes install software-properties-common

# needed for PHP-7.1
apt-add-repository --yes ppa:ondrej/php
apt-get --yes update

# set frontend to noninteractive (so mysql-server wont ask for root pw, timezone, and so on)
DEBIAN_FRONTEND_OLD=($DEBIAN_FRONTEND)
export DEBIAN_FRONTEND="noninteractive"

# main installation
# (use "quiet" so mysql hopefully doesnt ask for a root password, geographic area and timezone)
apt-get --quiet --yes --allow-unauthenticated install \
    curl wget \
    ant clang composer cron gcc git make openjdk-8-jdk sudo \
    apache2 mysql-server php7.1 php7.1-dev php7.1-gd php7.1-intl php7.1-json php7.1-ldap php7.1-mbstring php7.1-mcrypt php7.1-mysql php7.1-xsl php-pear libapache2-mod-gnutls libapache2-mod-php7.1 \
    ca-certificates libarchive-dev libboost-all-dev libcurl4-openssl-dev libkyotocabinet-dev liblept5 liblz4-tool libmagic-dev libmysqlclient-dev libopenjpeg5 libpcre3-dev libpoppler58 libssl-dev libtokyocabinet-dev libwebp5 libxml2-dev mawk poppler-utils uuid-dev \
    tesseract-ocr tesseract-ocr-bul tesseract-ocr-ces tesseract-ocr-dan tesseract-ocr-deu tesseract-ocr-eng tesseract-ocr-fin tesseract-ocr-fra tesseract-ocr-grc tesseract-ocr-heb tesseract-ocr-hun tesseract-ocr-ita tesseract-ocr-lat tesseract-ocr-nld tesseract-ocr-nor tesseract-ocr-pol tesseract-ocr-por tesseract-ocr-rus tesseract-ocr-slv tesseract-ocr-spa tesseract-ocr-swe

# create /var/run/mysqld and change user (mysql installation right now has a bug not doing that itself)
# (chown needs to be done after installation = after the user has been created)
export DEBIAN_FRONTEND=(DEBIAN_FRONTEND_OLD)
mkdir -p /var/run/mysqld
chown -R mysql:mysql /var/run/mysqld

a2enmod rewrite
phpenmod mcrypt
/etc/init.d/apache2 restart
