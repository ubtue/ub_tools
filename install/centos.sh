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


cd /etc/yum.repos.d/
wget http://download.opensuse.org/repositories/security:shibboleth/CentOS_7/security:shibboleth.repo
yum -y update
yum -y install epel-release awk git mariadb mariadb-server httpd php php-devel php-mcrypt php-intl php-ldap \
    php-mysql php-xsl php-gd php-mbstring php-mcrypt java-*-openjdk-devel mawk mod_ssl epel-release wget \
    policycoreutils-python curl-openssl mutt golang lsof clang gcc-c++.x86_64 file-devel pcre-devel openssl-devel \
    kyotocabinet-devel tokyocabinet-devel poppler-utils libwebp mariadb-devel.x86_64 libxml2-devel.x86_64 \
    libcurl-openssl-devel.x86_64 ant lz4 unzip libarchive-devel boost-devel cryptsetup
systemctl start mariadb.service


git clone --recurse-submodules https://github.com/ubtue/ub_tools.git /usr/local/ub_tools


if [[ $1 == "ixtheo" ]]; then
    git clone https://github.com/ubtue/ixtheo.git /usr/local/vufind
else # krimdok
    git clone https://github.com/ubtue/krimdok.git /usr/local/vufind
fi


cd /usr/local/ub_tools
make install
./cpp/installer $1
