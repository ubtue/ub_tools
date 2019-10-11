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

# additional repos (shibboleth = libcurl-openssl-devel, Alexander_Pozdnyakov = tesseract)
dnf config-manager --add-repo https://raw.githubusercontent.com/ubtue/ub_tools/master/cpp/data/installer/elasticsearch.repo
dnf config-manager --add-repo http://download.opensuse.org/repositories/security:shibboleth/CentOS_7/security:shibboleth.repo
dnf config-manager --add-repo https://download.opensuse.org/repositories/home:/Alexander_Pozdnyakov/CentOS_8/
rpm --import https://build.opensuse.org/projects/home:Alexander_Pozdnyakov/public_key
dnf --assumeyes update

# basic dependencies
InstallIfMissing "ca-certificates"
dnf --assumeyes install \
    ant bc cifs-utils clang crontabs ftp gcc-c++ git java-*-openjdk-devel make python3 sudo \
    curl-openssl gawk kyotocabinet kyotocabinet-devel libcurl-openssl-devel libdb-devel libsq3-devel libuuid-devel libwebp libxml2-devel libxml2 lsof lz4 mariadb mariadb-devel mariadb-server mod_ssl mysql-utilities openjpeg-libs openssl-devel pcre-devel policycoreutils-python-utils poppler poppler-utils rpmdevtools unzip xerces-c-devel \
    tesseract tesseract-devel tesseract-langpack-bul tesseract-langpack-ces tesseract-langpack-dan tesseract-langpack-deu tesseract-langpack-eng tesseract-langpack-fin tesseract-langpack-fra tesseract-langpack-grc tesseract-langpack-heb tesseract-langpack-hun tesseract-langpack-ita tesseract-langpack-lat tesseract-langpack-nld tesseract-langpack-nor tesseract-langpack-pol tesseract-langpack-por tesseract-langpack-rus tesseract-langpack-slv tesseract-langpack-spa tesseract-langpack-swe

# PowerTools repo
dnf --assumeyes --enablerepo=PowerTools install file-devel leptonica-devel libarchive-devel

### TUEFIND ###
if [[ $1 == "ixtheo" || $1 == "krimdok" ]]; then
    ColorEcho "installing/updating tuefind dependencies..."

    if [[ $1 == "krimdok" ]]; then
        InstallIfMissing elasticsearch
    fi

    # PHP
    dnf --assumeyes install php php-cli php-common php-gd php-intl php-ldap php-mbstring php-mysqlnd php-soap php-xml
    systemctl restart httpd

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
