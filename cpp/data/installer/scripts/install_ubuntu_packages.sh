#!/bin/bash
if [[ $# > 1 ]]; then
    echo "usage: $0 [system_type]"
    echo "          tuefind: Also install PHP/Apache/MySQL + JDK"
    exit 1
fi

function ColorEcho {
    echo -e "\033[1;34m" $1 "\033[0m"
}

#--------------------------------- UB_TOOLS ---------------------------------#
ColorEcho "installing/updating ub_tools dependencies..."

apt-get --yes update

# install software-properties-common for apt-add-repository
apt-get --yes install software-properties-common

# needed for PHP-7.1
apt-add-repository --yes ppa:ondrej/php
apt-get --yes update

# main installation
# (use "quiet" so mysql hopefully doesnt ask for a root password, geographic area and timezone)
apt-get --quiet --yes --allow-unauthenticated install \
    curl wget \
    ant cifs-utils clang cron gcc git make sudo \
    ca-certificates libarchive-dev libboost-all-dev libcurl4-gnutls-dev libkyotocabinet-dev liblept5 libleptonica-dev liblz4-tool libmagic-dev libmysqlclient-dev libpcre3-dev libpoppler68 libssl-dev libtesseract-dev libtokyocabinet-dev libwebp5 libxml2-dev libxml2-utils mawk poppler-utils uuid-dev \
    tesseract-ocr tesseract-ocr-bul tesseract-ocr-ces tesseract-ocr-dan tesseract-ocr-deu tesseract-ocr-eng tesseract-ocr-fin tesseract-ocr-fra tesseract-ocr-grc tesseract-ocr-heb tesseract-ocr-hun tesseract-ocr-ita tesseract-ocr-lat tesseract-ocr-nld tesseract-ocr-nor tesseract-ocr-pol tesseract-ocr-por tesseract-ocr-rus tesseract-ocr-slv tesseract-ocr-spa tesseract-ocr-swe

#---------------------------------- TUEFIND ---------------------------------#
if [[ $1 == "tuefind" ]]; then
    ColorEcho "installing/updating tuefind dependencies..."

    # set frontend to noninteractive (so mysql-server wont ask for root pw, timezone, and so on)
    DEBIAN_FRONTEND_OLD=($DEBIAN_FRONTEND)
    export DEBIAN_FRONTEND="noninteractive"

    apt-get --quiet --yes install \
        composer openjdk-8-jdk \
        apache2 mysql-server \
        php7.1 php7.1-curl php7.1-dev php7.1-gd php7.1-intl php7.1-json php7.1-ldap php7.1-mbstring php7.1-mcrypt php7.1-mysql php7.1-xsl php-pear \
        libapache2-mod-gnutls libapache2-mod-php7.1

    # create /var/run/mysqld and change user (mysql installation right now has a bug not doing that itself)
    # (chown needs to be done after installation = after the user has been created)

    export DEBIAN_FRONTEND=(DEBIAN_FRONTEND_OLD)
    mkdir -p /var/run/mysqld
    chown -R mysql:mysql /var/run/mysqld

    a2enmod rewrite
    phpenmod mcrypt
    /etc/init.d/apache2 restart
fi

ColorEcho "finished installing/updating dependencies"
