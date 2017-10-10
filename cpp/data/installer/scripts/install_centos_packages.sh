#!/bin/bash
# epel-release needs to be installed first, else packages like kyotocabinet won't be found
yum -y install curl epel-release wget

# additional repos (shibboleth = libcurl-openssl-devel.x86_64)
cd /etc/yum.repos.d/
wget http://download.opensuse.org/repositories/security:shibboleth/CentOS_7/security:shibboleth.repo
yum -y update

# basic dependencies
yum -y install \
    ant clang gcc-c++.x86_64 git java-*-openjdk-devel make \
    httpd mariadb mod_ssl \
    boost-devel ca-certificates curl-openssl file-devel kyotocabinet-devel leptonica libarchive-devel libcurl-openssl-devel libuuid-devel libwebp libxml2-devel.x86_64 lsof lz4 mariadb-devel.x86_64 mawk openjpeg-libs openssl-devel pcre-devel policycoreutils-python poppler poppler-utils tokyocabinet-devel unzip \
    tesseract tesseract-langpack-bul tesseract-langpack-ces tesseract-langpack-dan tesseract-langpack-deu tesseract-langpack-fin tesseract-langpack-fra tesseract-langpack-grc tesseract-langpack-heb tesseract-langpack-hun tesseract-langpack-ita tesseract-langpack-lat tesseract-langpack-nld tesseract-langpack-nor tesseract-langpack-pol tesseract-langpack-por tesseract-langpack-rus tesseract-langpack-slv tesseract-langpack-spa tesseract-langpack-swe

# in centos, there is no "tesseract-langpack-eng", it seems to be part of the default installation

# special handling for php+composer: standard php needs to be replaced by php71w
# (standard is installed as dependancy)
if yum list installed php71w-common | grep -q php71w-common; then
    echo "PHP 7.1 already installed"
else
    echo "replacing standard PHP with PHP 7.1"
    rpm -Uvh https://mirror.webtatic.com/yum/el7/webtatic-release.rpm
    yum -y remove 'php-*'
    yum -y install php71w-cli php71w-common php71w-devel php71w-gd php71w-intl php71w-ldap php71w-mbstring php71w-mcrypt php71w-mysqlnd php71w-xml mod_php71w
    systemctl restart httpd
fi

# composer also needs to be installed manually to avoid php dependency problems
if [ -e /usr/local/bin/composer ]; then
    echo "composer already installed"
else
    echo "installing composer"
    wget -O /tmp/composer-setup.php https://getcomposer.org/installer
    php /tmp/composer-setup.php --install-dir=/usr/local/bin --filename=composer
fi
