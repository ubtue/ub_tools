#!/bin/bash
add-apt-repository -y ppa:ubuntu-lxc/lxd-stable # Needed to get a recent version of Go.
apt update

apt-get --yes install \
    curl wget \
    software-properties-common \
    ant clang composer gcc git make openjdk-8-jdk \
    apache2 mysql-server php7.1 php7.1-dev php7.1-gd php7.1-intl php7.1-json php7.1-ldap php7.1-mcrypt php7.1-mysql php7.1-xsl php-pear libapache2-mod-gnutls libapache2-mod-php7.1 \
    ca-certificates libarchive-dev libboost-all-dev libcurl4-openssl-dev libkyotocabinet-dev libleptonica liblz4-tool libmagic-dev libmysqlclient-dev libopenjpeg5 libpcre3-dev libpoppler58 libssl-dev libtokyocabinet-dev libwebp5 libxml2-dev mawk poppler-utils uuid-dev \
    tesseract-ocr tesseract-ocr-bul tesseract-ocr-ces tesseract-ocr-dan tesseract-ocr-deu tesseract-ocr-eng tesseract-ocr-fin tesseract-ocr-fra tesseract-ocr-grc tesseract-ocr-heb tesseract-ocr-hun tesseract-ocr-ita tesseract-ocr-lat tesseract-ocr-nld tesseract-ocr-nor tesseract-ocr-pol tesseract-ocr-por tesseract-ocr-rus tesseract-ocr-slv tesseract-ocr-spa tesseract-ocr-swe

a2enmod rewrite
phpenmod mcrypt
/etc/init.d/apache2 restart
