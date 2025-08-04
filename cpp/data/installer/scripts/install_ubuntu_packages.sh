#!/bin/bash

export JAVA_TOOL_OPTIONS="-Dfile.encoding=UTF8"

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
curl -fsSL https://artifacts.elastic.co/GPG-KEY-elasticsearch | gpg --dearmor -o /etc/apt/keyrings/elastic-archive-keyring.gpg
apt-add-repository --yes --update 'deb https://artifacts.elastic.co/packages/8.x/apt stable main'
apt-add-repository --yes --update 'ppa:alex-p/tesseract-ocr5'


# main installation
apt-get --quiet --yes --allow-unauthenticated install \
        ant apache2 apparmor-utils ca-certificates cifs-utils clang clang-format cron curl gcc git imagemagick incron jq libarchive-dev \
        libcurl4-gnutls-dev libdb-dev liblept5 libleptonica-dev liblz4-tool libmagic-dev libmysqlclient-dev \
        libpcre3-dev libpq-dev libsqlite3-dev libssl-dev libstemmer-dev libtesseract-dev libwebp7 libxerces-c-dev \
        libxml2-dev libxml2-utils locales-all make mawk mutt nlohmann-json3-dev openjdk-17-jdk p7zip-full poppler-utils postgresql-client \
        python3 python3-paramiko \
        tesseract-ocr tesseract-ocr-all rsync sqlite3 tcl-expect-dev tidy unzip mpack \
        uuid-dev xsltproc libsystemd-dev libboost-all-dev

# Explicitly enable mod_cgi. If we would use `a2enmod cgi`, it would enable mod_cgid, which would fail on apache startup.
a2enmod cgi

# Set java version 17 to be kept manually (to avoid automatic migrations)
update-java-alternatives --set java-1.17.0-openjdk-amd64

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
    # 22.04 uses 8.1 by default, but we want to use 8.3 due to longer support period
    # Also, we use php-fpm with fcgi instead of libapache2-mod-php to avoid HTTP/2 compatibility issues with mpm_prefork.
    add-apt-repository --yes --update ppa:ondrej/php
    apt-get --quiet --yes install \
        composer npm node-grunt-cli \
        php8.3 php8.3-curl php8.3-gd php8.3-intl php8.3-ldap php8.3-mbstring php8.3-mysql php8.3-soap php8.3-xml \
        php8.3-fpm

    update-alternatives --set php /usr/bin/php8.3

    a2dismod mpm_prefork
    a2enmod mpm_event proxy_fcgi http2 rewrite setenvif ssl
    a2enconf php8.3-fpm
    /etc/init.d/apache2 restart
fi

ColorEcho "finished installing/updating dependencies"
