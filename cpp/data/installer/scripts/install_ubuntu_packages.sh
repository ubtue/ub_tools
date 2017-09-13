#!/bin/bash
apt install -y software-properties-common
add-apt-repository ppa:ubuntu-lxc/lxd-stable # Needed to get a recent version of Go.
apt update
apt install -y clang golang wget curl
apt install -y git apache2 libapache2-mod-gnutls php7.0 php7.0-dev php-pear php7.0-json php7.0-ldap php7.0-mcrypt php7.0-mysql php7.0-xsl php7.0-intl php7.0-gd libapache2-mod-php7.0 composer openjdk-8-jdk libmagic-dev libpcre3-dev libssl-dev libkyotocabinet-dev mutt libxml2-dev libmysqlclient-dev libcurl4-openssl-dev ant libtokyocabinet-dev liblz4-tool libarchive-dev libboost-all-dev clang-3.8 clang++-3.8 clang golang uuid-dev
a2enmod rewrite
phpenmod mcrypt
/etc/init.d/apache2 restart
