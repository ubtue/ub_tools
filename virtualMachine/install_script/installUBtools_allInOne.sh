#!/bin/bash


##################
# script for installation of ub-tools
# os: ubuntu 22.04
# by Steven Lolong (steven.lolong@uni-tuebingen.de)
# based on the script for Dockter maintained by by Mario Trojan
# ******************
##################


#######################
#prerequisites:
# 1. copy "installUBtools_allInOne.sh" into /tmp/
# 2. change the file "installUBtools_allInOne.sh" mode to 700
# 3. parameter option:
#   < fulltext-backend | ub-tools-only | vufind < ixtheo | krimdok > > <--test | --production> [--omit-cronjobs] [--omit-systemctl]
#   - example:
#     /tmp/installUBtools_allInOne.sh vufind ixtheo --test --omit-cronjobs | tee ~/output.log

# note:
# - for ixtheo minimum RAM=8GB and krimdok=4GB
# - for ixtheo | krimdok, don't forget to copy example auth.mrc and biblio.mrc to folder /tmp
#######################

function ColorEcho {
    echo -e "\033[1;34m" $1 "\033[0m"
}

ColorEcho "*************** Starting installation of vufind/tuefind for ixtheo *******************"

ColorEcho "installation -> create and copy directory, and export java environment"

# mkdir --parent /mnt/ZE020110/FID-Projekte

# cp -rf /media/sf_iiplo01/Documents/ub_tools/docker/ixtheo/mnt/ZE020150 /mnt/

# cp /media/sf_iiplo01/Documents/ub_tools/docker/ixtheo/mnt/.smbcredentials /root/.smbcredentials
# cp /mnt/.smbcredentials /root/.smbcredentials

cd /
export JAVA_TOOL_OPTIONS="-Dfile.encoding=UTF8"

apt-get --yes update \
    && apt-get --yes install sudo curl \
    && apt-get --yes install sudo wget \
    && apt-get --yes install sudo git

# make sure we are on ubuntu
if [ -e /etc/debian_version ]; then
    #print ubuntu version
    lsb_release -a
else
    ColorEcho "installation -> OS type could not be detected or is not supported! aborting"
    exit 1
fi

# check prerequisites and invariants
if [ "$(id -u)" != "0" ]; then
    ColorEcho "This script must be run as root" 1>&2
    exit 1
fi
if [[ ! $PATH =~ "/usr/local/bin" ]]; then
    ColorEcho "installation -> please add /usr/local/bin to your PATH before starting the installation!"
    exit 1
fi

if [ -z "$BRANCH" ]; then
    BRANCH="ubuntu2204_slolong"
fi
ColorEcho "Branch is: ${BRANCH}"


#--------------------------------- UB_TOOLS ---------------------------------#
ColorEcho "installation -> installing/updating ub_tools dependencies..."


# install software-properties-common for apt-add-repository
apt-get --yes install software-properties-common
wget -qO - https://artifacts.elastic.co/GPG-KEY-elasticsearch | sudo apt-key add -
apt-add-repository --yes --update 'deb https://artifacts.elastic.co/packages/7.x/apt stable main'
apt-add-repository --yes --update 'ppa:alex-p/tesseract-ocr5'



# main installation
apt-get --quiet --yes --allow-unauthenticated install \
        ant apache2 apparmor-utils ca-certificates cifs-utils clang clang-format cron curl gcc git imagemagick incron jq libarchive-dev \
        libcurl4-gnutls-dev libdb-dev liblept5 libleptonica-dev liblz4-tool libmagic-dev libmysqlclient-dev \
        libpcre3-dev libpq-dev libsqlite3-dev libssl-dev libstemmer-dev libtesseract-dev libwebp7 libxerces-c-dev \
        libxml2-dev libxml2-utils locales-all make mawk mutt nlohmann-json3-dev openjdk-11-jdk p7zip-full poppler-utils postgresql-client \
        tesseract-ocr tesseract-ocr-all rsync sqlite3 tcl-expect-dev tidy unzip \
        uuid-dev xsltproc libsystemd-dev

# Explicitly enable mod_cgi. If we would use `a2enmod cgi`, it would enable mod_cgid, which would fail on apache startup.
ln -s ../mods-available/cgi.load /etc/apache2/mods-enabled/cgi.load

# Set java version 11 to be kept manually (to avoid automatic migrations)
update-alternatives --set java /usr/lib/jvm/java-11-openjdk-amd64/bin/java
update-alternatives --set javac /usr/lib/jvm/java-11-openjdk-amd64/bin/javac

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

ColorEcho "finished installing/updating dependencies"

if [ -d /usr/local/ub_tools ]; then
    ColorEcho "installation -> ub_tools already exists, skipping download"
else
    ColorEcho "installation -> cloning ub_tools --branch ${BRANCH}"
    git clone --branch ${BRANCH} https://github.com/ubtue/ub_tools.git /usr/local/ub_tools
fi



ColorEcho "installation -> building prerequisites"
cd /usr/local/ub_tools/cpp/lib/mkdep && CCC=clang++ make --jobs=4 install

ColorEcho "installation -> building cpp installer"
cd /usr/local/ub_tools/cpp && CCC=clang++ make --jobs=4 installer

ColorEcho "installation -> starting cpp installer"
/usr/local/ub_tools/cpp/installer $*

ColorEcho "installation -> reload system configuration"
systemctl stop apache2 

if [[ $2 == "ixtheo" ]]; then
    ColorEcho "installation -> copy local_override"
    cp /usr/local/ub_tools/docker/ixtheo/local_overrides/* /usr/local/vufind/local/tuefind/local_overrides/

    ColorEcho "installation -> copying apache conf to sites-available"
    cp /usr/local/ub_tools/docker/ixtheo/apache2/*.conf /etc/apache2/sites-available/

    ColorEcho "installation -> copying ssl certificate"
    cp /usr/local/ub_tools/docker/ixtheo/apache2/*.pem /etc/ssl/certs/
fi

if [[ $2 == "krimdok" ]]; then
    ColorEcho "installation -> copy local_override"
    cp /usr/local/ub_tools/docker/krimdok/local_overrides/* /usr/local/vufind/local/tuefind/local_overrides/

    ColorEcho "installation -> copying apache conf to sites-available"
    cp /usr/local/ub_tools/docker/krimdok/apache2/*.conf /etc/apache2/sites-available/

    ColorEcho "installation -> copying ssl certificate"
    cp /usr/local/ub_tools/docker/krimdok/apache2/*.pem /etc/ssl/certs/

fi

if [[ $2 == "ixtheo" || $2 == "krimdok" ]]; then

    ColorEcho "installation -> update mysql.cnf"
    echo authentication_policy=mysql_native_password >> /etc/mysql/mysql.conf.d/mysqld.cnf

    ColorEcho "installation -> updating ownership of config and cache"
    chown -R www-data:www-data /usr/local/vufind/local/tuefind/local_overrides/*.conf

    chown -R www-data:www-data /usr/local/vufind/local/tuefind/instances/$2/cache
    chmod -R 775 /usr/local/vufind/local/tuefind/instances/$2/cache


    chown -R www-data:www-data /usr/local/vufind/local/cache

    ColorEcho "installation -> creating synonyms"
    /usr/local/vufind/solr/vufind/biblio/conf/touch_synonyms.sh $2
    chown -R solr:solr /usr/local/vufind/solr/vufind/biblio/conf/synonyms

    ColorEcho "installation -> restarting mysql server"
    systemctl restart mysql

    ColorEcho "installation -> copy *.mrc as example files"
    cp /mnt/ZE020150/FID-Entwicklung/IxTheo/mrc/*.mrc /usr/local/ub_tools/bsz_daten/

    ColorEcho "installation -> restart vufind"
    systemctl restart vufind 

    ColorEcho "installation -> running exporting .mrc file"
    . /etc/profile.d/vufind.sh \
        && /usr/local/vufind/import-marc.sh /usr/local/ub_tools/bsz_daten/biblio.mrc \
        && /usr/local/vufind/import-marc-auth.sh /usr/local/ub_tools/bsz_daten/auth.mrs


    ColorEcho "installation -> removing default apache website"
    rm /etc/apache2/sites-enabled/000-default.conf

    ColorEcho "installation -> updating site available"
    chmod 644 /etc/apache2/sites-available/*.conf

    # ColorEcho "installation -> creating softlink of vufind-vhost.conf"
    # ln -s /etc/apache2/sites-available/vufind-vhosts.conf /etc/apache2/sites-enabled/vufind-vhosts.conf

    ColorEcho "installation -> upadating site enable"
    a2ensite vufind-vhosts


    ColorEcho "installation -> run npm in vufind's folder"
    cd /usr/local/vufind && sudo npm install

    ColorEcho "installation -> run grunt in vufind's folder"
    cd /usr/local/vufind && grunt less

    ColorEcho "installation -> reload apache and vufind service"
    systemctl restart apache2
    systemctl restart vufind
    systemctl restart mysql
fi

if [[ $1 == "fulltext-backend" ]]; then
    su  -c /usr/share/elasticsearch/bin/elasticsearch -s /bin/bash elasticsearch
fi

if [[ $1 == "ub-tools-only" ]]; then

    ColorEcho "installation -> update mysql.cnf"
    echo authentication_policy=mysql_native_password >> /etc/mysql/mysql.conf.d/mysqld.cnf

    ColorEcho "installation -> copying apache conf to sites-available"
    cp /usr/local/ub_tools/docker/ub_tools/apache2/*.conf /etc/apache2/sites-available/

    ColorEcho "installation -> copying ssl certificate"
    cp /usr/local/ub_tools/docker/ub_tools/apache2/*.pem /etc/ssl/certs/

    ColorEcho "installation -> updating site available"
    chmod 644 /etc/apache2/sites-available/*.conf

    ColorEcho "installation -> upadating site enable"
    a2ensite ub_tools-vhosts

    ColorEcho "installation -> add ssl module to apache"
    a2enmod ssl
    ColorEcho "installation -> reload apache server"
    systemctl restart apache2
    systemctl restart mysql
fi