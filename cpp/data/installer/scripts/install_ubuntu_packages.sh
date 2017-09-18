#!/bin/bash
apt-get --yes install software-properties-common
add-apt-repository -y ppa:ubuntu-lxc/lxd-stable # Needed to get a recent version of Go.
apt update
apt-get --yes install curl wget
apt-get --yes install ant clang golang openjdk-8-jdk
apt-get --yes install apache2 php7.0 php7.0-dev php7.0-gd php7.0-intl php7.0-json php7.0-ldap php7.0-mcrypt php7.0-mysql php7.0-xsl php-pear libapache2-mod-gnutls libapache2-mod-php7.0
apt-get --yes install libarchive-dev libboost-all-dev libcurl4-openssl-dev libkyotocabinet-dev liblz4-tool libmagic-dev libmysqlclient-dev libpcre3-dev libssl-dev libtokyocabinet-dev libxml2-dev uuid-dev
apt-get --yes install composer git make
#@ruschein: how to do silent mutt (+postfix?) installation?
#apt install -y mutt
a2enmod rewrite
phpenmod mcrypt
/etc/init.d/apache2 restart

apt-get --yes ca-certificates libleptonica libopenjpeg5 libpoppler58 libwebp5 poppler-utils
apt-get --yes install tesseract-ocr tesseract-ocr-bul tesseract-ocr-ces tesseract-ocr-dan tesseract-ocr-deu tesseract-ocr-eng tesseract-ocr-fin tesseract-ocr-fra tesseract-ocr-grc tesseract-ocr-heb tesseract-ocr-hun tesseract-ocr-ita tesseract-ocr-lat tesseract-ocr-nld tesseract-ocr-nor tesseract-ocr-pol tesseract-ocr-por tesseract-ocr-rus tesseract-ocr-slv tesseract-ocr-spa tesseract-ocr-swe
