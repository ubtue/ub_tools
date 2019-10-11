#!/bin/bash
if [[ $# > 1 ]]; then
    echo "usage: $0 [system_type]"
    echo "          ixtheo|krimdok: Also install specific dependencies"
    exit 1
fi

function ColorEcho {
    echo -e "\033[1;34m" $1 "\033[0m"
}

function InstallIfMissing {
    if dnf list installed $1 | grep --quiet $1; then
        ColorEcho "\"$1\" already installed"
    else
        dnf --assumeyes install $1
    fi
}

if [[ $1 != "" && $1 != "ixtheo" && $1 != "krimdok" ]]; then
    ColorEcho "invalid system_type \"$1\"!"
    exit 1
fi

#--------------------------------- UB_TOOLS ---------------------------------#
ColorEcho "installing/updating ub_tools dependencies..."

# make sure dnf config-manager plugin is installed (Docker)
dnf --assumeyes install dnf-plugins-core

# epel-release needs to be installed first, else packages like kyotocabinet won't be found
dnf --assumeyes install curl epel-release wget

# additional repos (shibboleth = libcurl-openssl-devel)
cd /etc/yum.repos.d/
wget --timestamping http://download.opensuse.org/repositories/security:shibboleth/CentOS_7/security:shibboleth.repo
wget --timestamping https://raw.githubusercontent.com/ubtue/ub_tools/master/cpp/data/installer/elasticsearch.repo
dnf --assumeyes update

# tesseract repo
dnf config-manager --add-repo https://download.opensuse.org/repositories/home:/Alexander_Pozdnyakov/CentOS_8/
rpm --import https://build.opensuse.org/projects/home:Alexander_Pozdnyakov/public_key

# basic dependencies
InstallIfMissing "ca-certificates"
dnf --assumeyes install \
    ant bc cifs-utils clang crontabs ftp gcc-c++ git java-*-openjdk-devel make sudo \
    curl-openssl file-devel gawk kyotocabinet kyotocabinet-devel leptonica libarchive-devel libcurl-openssl-devel libsq3-devel libuuid-devel libwebp libxml2-devel libxml2 lsof lz4 mariadb mariadb-devel mariadb-server mod_ssl mysql-utilities openjpeg-libs openssl-devel pcre-devel policycoreutils-python poppler poppler-utils unzip xerces-c-devel \
    tesseract tesseract-devel tesseract-langpack-bul tesseract-langpack-ces tesseract-langpack-dan tesseract-langpack-deu tesseract-langpack-eng tesseract-langpack-fin tesseract-langpack-fra tesseract-langpack-grc tesseract-langpack-heb tesseract-langpack-hun tesseract-langpack-ita tesseract-langpack-lat tesseract-langpack-nld tesseract-langpack-nor tesseract-langpack-pol tesseract-langpack-por tesseract-langpack-rus tesseract-langpack-slv tesseract-langpack-spa tesseract-langpack-swe rpmdevtools python3 libdb-devel


### TUEFIND ###
if [[ $1 == "ixtheo" || $1 == "krimdok" ]]; then
    ColorEcho "installing/updating tuefind dependencies..."

    if [[ $1 == "krimdok" ]]; then
        InstallIfMissing elasticsearch
    fi

    # special handling for php+composer: standard php needs to be replaced by php71w
    # (standard is installed as dependancy)
    if dnf list installed php71w-common | grep --quiet php71w-common; then
        ColorEcho "PHP 7.1 already installed"
    else
        ColorEcho "replacing standard PHP with PHP 7.1"
        rpm -Uvh https://mirror.webtatic.com/yum/el7/webtatic-release.rpm
        dnf --assumeyes remove 'php-*'
        dnf --assumeyes install php71w-cli php71w-common php71w-gd php71w-intl php71w-ldap php71w-mbstring php71w-mysqlnd php71w-soap php71w-xml mod_php71w
        systemctl restart httpd
    fi

    # composer also needs to be installed manually to avoid php dependency problems
    if [ -e /usr/local/bin/composer ]; then
        ColorEcho "composer already installed"
    else
        ColorEcho "installing composer"
        wget --output-document=/tmp/composer-setup.php https://getcomposer.org/installer
        php /tmp/composer-setup.php --install-dir=/usr/local/bin --filename=composer
    fi
fi

ColorEcho "finished installing/updating dependencies"
