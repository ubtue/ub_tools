#!/bin/bash
if [[ $# > 1 ]]; then
    echo "usage: $0 [system_type]"
    echo "          ixtheo|krimdok: Also install specific dependencies"
    exit 1
fi

function ColorEcho {
    echo -e "\033[1;34m" $1 "\033[0m"
}

if [[ $1 != "" && $1 != "ixtheo" && $1 != "krimdok" && $1 != "fulltext_backend" ]]; then
    ColorEcho "invalid system_type \"$1\"!"
    exit 1
fi

#--------------------------------- UB_TOOLS ---------------------------------#
ColorEcho "installing/updating ub_tools dependencies..."

apt-get --yes update

# install additional libraries for docker environment
apt-get --yes install sudo wget

# install software-properties-common for apt-add-repository
apt-get --yes install software-properties-common
wget -qO - https://artifacts.elastic.co/GPG-KEY-elasticsearch | sudo apt-key add -
apt-add-repository --yes --update 'deb https://artifacts.elastic.co/packages/7.x/apt stable main'

# main installation
apt-get --quiet --yes --allow-unauthenticated install \
        ant apache2 apparmor-utils ca-certificates cifs-utils clang clang-format-12 cron curl gcc git imagemagick incron jq libarchive-dev \
        libcurl4-gnutls-dev libdb-dev liblept5 libleptonica-dev liblz4-tool libmagic-dev libmysqlclient-dev \
        libpcre3-dev libpq-dev libsqlite3-dev libssl-dev libstemmer-dev libtesseract-dev libwebp6 libxerces-c-dev \
        libxml2-dev libxml2-utils locales-all make mawk mutt openjdk-8-jdk poppler-utils postgresql-client \
        rsync sqlite3 tcl-expect-dev tesseract-ocr tesseract-ocr-bul tesseract-ocr-ces tesseract-ocr-dan \
        tesseract-ocr-deu tesseract-ocr-eng tesseract-ocr-fin tesseract-ocr-fra tesseract-ocr-heb tesseract-ocr-hun \
        tesseract-ocr-ita tesseract-ocr-lat tesseract-ocr-nld tesseract-ocr-nor tesseract-ocr-pol tesseract-ocr-por \
        tesseract-ocr-rus tesseract-ocr-script-grek tesseract-ocr-slv tesseract-ocr-spa tesseract-ocr-swe tidy unzip \
        uuid-dev xsltproc libsystemd-dev

# Explicitly enable mod_cgi. If we would use `a2enmod cgi`, it would enable mod_cgid, which would fail on apache startup.
ln -s ../mods-available/cgi.load /etc/apache2/mods-enabled/cgi.load

# From 18.04 on, Java 8 needs to be enabled as well for Solr + mixins (18.04 ships with 10)
# (unfortunately, >= string comparison is impossible in Bash, so we compare > 17.10)
. /etc/lsb-release
if [[ $DISTRIB_RELEASE > "17.10" ]]; then
    update-alternatives --set java /usr/lib/jvm/java-8-openjdk-amd64/jre/bin/java
fi

#Install custom certificates
mkdir --parents /usr/share/ca-certificates/custom
cp /usr/local/ub_tools/docker/zts/extra_certs/extra_certs.pem /usr/share/ca-certificates/custom/eguzkilore.crt
grep --quiet --extended-regexp --line-regexp  '[!]?custom[/]eguzkilore.crt' /etc/ca-certificates.conf \
        || echo 'custom/eguzkilore.crt' >> /etc/ca-certificates.conf
dpkg-reconfigure --frontend=noninteractive ca-certificates


#mysql installation
## (use "quiet" and set frontend to noninteractive so mysql doesnt ask for a root password, geographic area and timezone)
DEBIAN_FRONTEND_OLD=($DEBIAN_FRONTEND)
export DEBIAN_FRONTEND="noninteractive"
apt-get --quiet --yes --allow-unauthenticated install mysql-server
export DEBIAN_FRONTEND=(DEBIAN_FRONTEND_OLD)
## create /var/run/mysqld and change user (mysql installation right now has a bug not doing that itself)
## (chown needs to be done after installation = after the user has been created)
mkdir --parents /var/run/mysqld
chown --recursive mysql:mysql /var/run/mysqld

#----------------------------------ELASTICSEARCH-----------------------------#
if [[ $1 == "krimdok" || $1 == "fulltext_backend" ]]; then
    apt-get --quiet --yes install elasticsearch
    if ! /usr/share/elasticsearch/bin/elasticsearch-plugin list | grep --quiet analysis-icu; then
        /usr/share/elasticsearch/bin/elasticsearch-plugin install analysis-icu
    fi
    mkdir --parents /etc/elasticsearch/synonyms
    for i in all de en fr it es pt ru el hans hant; do touch /etc/elasticsearch/synonyms/synonyms_$i.txt; done
fi

#---------------------------------- TUEFIND ---------------------------------#
if [[ $1 == "ixtheo" || $1 == "krimdok" ]]; then
    ColorEcho "installing/updating tuefind dependencies..."
    apt-get --quiet --yes install \
        composer \
        php php-curl php-gd php-intl php-json php-ldap php-mbstring php-mysql php-pear php-soap php-xml \
        libapache2-mod-php

    a2enmod rewrite
    a2enmod ssl
    /etc/init.d/apache2 restart
fi

ColorEcho "finished installing/updating dependencies"
