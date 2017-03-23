#!/bin/bash
set -o errexit -o nounset


if [[ $# != 1 ]] || [[ $1 != "krimdok" && $1 != "ixtheo" ]]; then
    echo "Usage: $0 (krimdok|ixtheo)"
    exit 1
fi


if [[ $(whoami) != "root" ]]; then
    echo "You must execute this script as root!"
    exit 1
fi


add-apt-repository --yes ppa:ubuntu-lxc/lxd-stable # Needed to get a recent version of Go.
apt -y update
apt -y install clang golang wget curl git apache2 libapache2-mod-gnutls mysql-server php7.0 php7.0-dev \
    php-pear php7.0-json php7.0-ldap php7.0-mcrypt php7.0-mysql php7.0-xsl php7.0-intl php7.0-gd \
    libapache2-mod-php7.0 composer openjdk-8-jdk libmagic-dev libpcre3-dev libssl-dev libkyotocabinet-dev mutt \
    libxml2-dev libmysqlclient-dev libcurl4-openssl-dev ant libtokyocabinet-dev liblz4-tool libarchive-dev \
    libboost-all-dev clang-3.8 clang++-3.8 clang golang
a2enmod rewrite
phpenmod mcrypt
/etc/init.d/apache2 restart


git clone https://github.com/ubtue/ub_tools.git /usr/local/ub_tools


if [[ $1 == "ixtheo" ]]; then
    git clone https://github.com/ubtue/ixtheo.git /usr/local/vufind
else # krimdok
    git clone https://github.com/ubtue/krimdok.git /usr/local/vufind
fi


cd /usr/local/ub_tools/cpp/lib/mkdep
make install
cd ../..
make installer
./installer $1
