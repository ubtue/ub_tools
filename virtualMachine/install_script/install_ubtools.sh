#!/bin/bash


##################
# script for installation of ub-tools
# os: ubuntu 22.04
# by Steven Lolong (steven.lolong@uni-tuebingen.de)
# based on the script maintained by by Mario Trojan
# ******************
# run this script on Virtual machine
# /tmp/install_ubtools.sh vufind ixtheo --test --omit-cronjobs 2>&1 | tee ~/outputfile.txt
# ***********************
# run this script on docker "Dockerfile"
# RUN /tmp/install_ubtools.sh vufind ixtheo --test --omit-cronjobs --omit-systemctl
##################


#######################
#prerequisites:
# 1. copy "install_dep_machine.sh" into /tmp/
# 2. change the file "install_dep_machine.sh" mode to 700
# ************************** 
# 1. create requirement folder
# 2. copy all existing data into the folder created at step-1
# 3. copy smb credentials into /root folder 
# 4. export java environment
# 5. update existing machine 
# 6. run script "install_dep_machine.sh"
# 7.  
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

export JAVA_TOOL_OPTIONS="-Dfile.encoding=UTF8"

apt-get --yes update

# install additional 
apt-get --yes install sudo git


# make sure we are on ubuntu
if [ -e /etc/debian_version ]; then
    #print ubuntu version
    lsb_release -a
else
    ColorEcho "OS type could not be detected or is not supported! aborting"
    exit 1
fi

# check prerequisites and invariants
if [ "$(id -u)" != "0" ]; then
    ColorEcho "This script must be run as root" 1>&2
    exit 1
fi
if [[ ! $PATH =~ "/usr/local/bin" ]]; then
    ColorEcho "Please add /usr/local/bin to your PATH before starting the installation!"
    exit 1
fi

if [ -z "$BRANCH" ]; then
    BRANCH="ubuntu2204_slolong"
fi
ColorEcho "Branch is: ${BRANCH}"


/tmp/install_dep_machine.sh

if [ -d /usr/local/ub_tools ]; then
    ColorEcho "ub_tools already exists, skipping download"
else
    ColorEcho "cloning ub_tools --branch ${BRANCH}"
    git clone --branch ${BRANCH} https://github.com/ubtue/ub_tools.git /usr/local/ub_tools
    # ColorEcho "installation -> copy from local ..."
    # cp -rf /media/sf_iiplo01/ub_tools_ubuntu2204_slolong /usr/local/ub_tools_ubuntu2204_slolong
    # mv /usr/local/ub_tools_ubuntu2204_slolong /usr/local/ub_tools
fi


ColorEcho "installation -> update mysql.cnf"
echo authentication_policy=mysql_native_password >> /etc/mysql/mysql.conf.d/mysqld.cnf

ColorEcho "installation -> building prerequisites"
cd /usr/local/ub_tools/cpp/lib/mkdep && CCC=clang++ make --jobs=4 install

ColorEcho "installation -> building cpp installer"
cd /usr/local/ub_tools/cpp && CCC=clang++ make --jobs=4 installer

ColorEcho "starting cpp installer"
/usr/local/ub_tools/cpp/installer $*

ColorEcho "installation -> reload system configuration"
systemctl stop apache2 

ColorEcho "installation -> copy local_override"
cp /usr/local/ub_tools/docker/ixtheo/local_overrides/* /usr/local/vufind/local/tuefind/local_overrides/

ColorEcho "installation -> updating ownership of config and cache"
chown -R www-data:www-data /usr/local/vufind/local/tuefind/local_overrides/*.conf

# chown -R www-data:www-data /usr/local/vufind/local/tuefind/instances/ixtheo/config
chown -R www-data:www-data /usr/local/vufind/local/tuefind/instances/$2/cache
# chmod 775 /usr/local/vufind/local/tuefind/instances/ixtheo/config
chmod -R 775 /usr/local/vufind/local/tuefind/instances/$2/cache


# chown -R www-data:www-data /usr/local/vufind/local/config
chown -R www-data:www-data /usr/local/vufind/local/cache
#/usr/local/vufind/local/tuefind/instances/bibstudies/config/vufind/local_overrides/

ColorEcho "installation -> creating synonyms"
/usr/local/vufind/solr/vufind/biblio/conf/touch_synonyms.sh $2
chown -R solr:solr /usr/local/vufind/solr/vufind/biblio/conf/synonyms

ColorEcho "installation -> restarting mysql server"
systemctl restart mysql

# ColorEcho "installation -> activating vufind user by login using mysql-client"
# cp /usr/local/usb_tools/docker/ixtheo/my_vu.cnf /root/
# cp /usr/local/usb_tools/docker/ixtheo/justquit.ans /root/
# mysql --defaults-extra-file=/root/my_vu.cnf < /root/justquit.ans

# rm /root/my_vu.cnf 
# rm /root/justquit.ans

ColorEcho "installation -> copy *.mrc as example files"
cp /tmp/*.mrc /usr/local/ub_tools/bsz_daten/

ColorEcho "installation -> restart vufind"
systemctl restart vufind 

ColorEcho "installation -> running exporting .mrc file"
. /etc/profile.d/vufind.sh \
    && /usr/local/vufind/import-marc.sh /usr/local/ub_tools/bsz_daten/biblio.mrc \
    && /usr/local/vufind/import-marc-auth.sh /usr/local/ub_tools/bsz_daten/auth.mrs
    # && sudo -u solr /usr/local/vufind/solr.sh stop\
    # && sudo -u solr /usr/local/vufind/solr.sh start \

ColorEcho "installation -> removing default apache website"
rm /etc/apache2/sites-enabled/000-default.conf

ColorEcho "installation -> copying ixtheo apache conf to sites-available"
cp /tmp/apache2/*.conf /etc/apache2/sites-available/
chmod 644 /etc/apache2/sites-available/*.conf

ColorEcho "installation -> creating softlink of vufind-vhost.conf"
ln -s /etc/apache2/sites-available/vufind-vhosts.conf /etc/apache2/sites-enabled/vufind-vhosts.conf
a2ensite vufind-vhosts

cp /tmp/apache2/*.pem /etc/ssl/certs/


ColorEcho "installation -> run npm in vufind's folder"
cd /usr/local/vufind && sudo npm install

ColorEcho "installation -> run grunt in vufind's folder"
cd /usr/local/vufind && grunt less

ColorEcho "installation -> reload apache and vufind service"
systemctl restart apache2
systemctl restart vufind
systemctl restart mysql