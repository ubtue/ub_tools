#!/bin/bash

show_help() {
  cat << EOF
Creates or updates a copy of vufind in the current working directory.

USAGES:
${0##*/} [OPTIONS] LocalName IP URL MODULES
${0##*/} --install

OPTIONS: (Each option must be seperated!)  
    -a HTPASSWD --auth HTPASSWD   Configures basic authentification
    -c          --clone           Clone git repository 
    -fs         --force-https     Configures apache to redirect http to https 
    -h          --help -?         Display this help and exit
    -n FILE     --norm FILE       Import Normdaten from FILE 
    -ü FILE     --über FILE       Import übergeordnete Daten from FILE
    -s CERT KEY --ssl CERT KEY    Configures HTTPS

    -i          --install         Install dependencies (only needed once)
    
ARGUMENTS:
    LocalName                     A unique name for this copy.
    IP                            The ip of this server (for apache/httpd)
    URL                           The url of this server (for apache/httpd)
    MODULES                       List of costumized VuFind modules
EOF
}

installDependencies() {
	sudo apt-get update
	sudo apt-get -y install apache2
	sudo a2enmod rewrite
	sudo /etc/init.d/apache2 force-reload
	sudo apt-get -y install mysql-server php5 php5-dev php-pear php5-json php5-ldap php5-mcrypt php5-mysql php5-xsl php5-intl php5-gd default-jdk

  if [ $? -ne 0 ]; then
    echo "ERROR. Please fix the problems and try again."
    exit 1;
  else 
    echo ""
    echo "Allmost done..."
    echo "Now you have to create a mysql user 'vufind' with the password"
    echo "Then you have to import the database from an existing instance"
    exit 0;
  fi
}

#$1 GIT_REPOSITORY
#$2 LOCAL_COPY_DIRECTORY
clone() {
	echo "CLONE..."
	git clone "$1" "$2"
	if [ $? -ne 0 ]; then
		echo "ERROR. Couldn't clone git repository '$1' to '$2'"
		exit 1;
	fi
}

# $1: LOCAL_COPY_DIRECTORY
# $2: LOCAL_COPY_DIRECTORY_LOCAL_DIR
setPrivileges() {
	USER=`whoami`
	GROUP=`groups | sed -r 's/ .*//g'`
	if [ "$GROUP" == "" ] ; then
		GROUP=$USER
	fi
  echo "SET PRIVILEGES..."
  sudo chown www-data:$GROUP $1
  sudo chmod +xr $1
  sudo chmod +xr $2
  sudo chown www-data:$GROUP $2/cache
  sudo chown www-data:$GROUP $2/config
  sudo chown www-data:$GROUP $2/logs/
  sudo touch $2/logs/record.xml
  sudo touch $2/logs/search.xml
  sudo touch /var/log/vufind.log
  sudo chown www-data:$GROUP /var/log/vufind.log
  sudo mkdir -p "$2/config/vufind/local_overrides"
  sudo chown $USER:$GROUP "$2/config/vufind/local_overrides"
  sudo chmod +xr "$2/config/vufind/local_overrides"

  # check for SELinux
  sestatus &> /dev/null
  if [ $? -eq 0 ]; then
    sudo chcon -h system_u:object_r:httpd_config_t:s0 $2/httpd-vufind.conf
    sudo chcon -R unconfined_u:object_r:httpd_sys_rw_content_t:s0 $1
    sudo chcon -R unconfined_u:object_r:httpd_sys_rw_content_t:s0 $2/cache
    sudo chcon -R unconfined_u:object_r:httpd_sys_rw_content_t:s0 $2/config
    sudo chcon system_u:object_r:httpd_config_t:s0 $2/httpd-vufind.conf
    sudo chcon system_u:object_r:httpd_config_t:s0 $2/httpd-vufind-vhosts.conf
    sudo chcon unconfined_u:object_r:httpd_sys_rw_content_t:s0 $2/logs/record.xml
    sudo chcon unconfined_u:object_r:httpd_sys_rw_content_t:s0 $2/logs/search.xml
    sudo chown www-data:www-data $2/import/solrmarc.log
    sudo chcon system_u:object_r:httpd_log_t:s0 /var/log/vufind.log
  fi
}

createEtcProfile() {
  echo "CREATE /etc/profile.d/vufind.sh..."
  # profile.d
  sudo sh -c "echo export JAVA_HOME=/usr/lib/jvm/default-java        > /etc/profile.d/vufind.sh"
  sudo sh -c "echo export VUFIND_HOME=/usr/local/vufind2            >> /etc/profile.d/vufind.sh"
  sudo sh -c "echo export VUFIND_LOCAL_DIR=/usr/local/vufind2/local >> /etc/profile.d/vufind.sh"
  sudo sh -c ". /etc/profile.d/vufind.sh"
}

# $1: SERVER_URL
# $2: Name
# $3: SITE_TARGET_FILE
# $4: DATABASE_TARGET_FILE
createServerConfig() {
  echo "CREATE SERVER CONFIG... "
  sh -c "> $3"
  echo "url   = \"https://$1\"" >> $3
  echo "email = \"support@$1\"" >> $3
  echo "title = \"$2\"" >> $3
  
  sh -c "> $4"
  echo "database = \"mysql://vufind:Roh8aeng@localhost/vufind\"" >> $4
}

#$1: SERVER_IP
#$2: SERVER_URL
#$3: TARGET
createVirtualHost() {
    target=$3
    echo "CREATE VIRTUAL HOST CONFIG... $target"
    sh -c "> $target"
    sh -c "echo 'ServerName $2'                              >> $target"
    sh -c "echo '<VirtualHost $1:80>'                        >> $target"
    sh -c "echo '    DocumentRoot /usr/local/vufind2/public' >> $target"
    sh -c "echo '    ServerName $2'                          >> $target"
    sh -c "echo '    ServerAlias $2'                         >> $target"
    sh -c "echo '</VirtualHost>'                             >> $target"
}

#$1: SERVER_IP
#$2: SERVER_URL
#$3: SSL-CERT
#$4: SSL-KEY
#$5: TARGET
createVirtualHostWithSSH() {
  echo "$1, $2, $3, $4, $5"
  createVirtualHost $1 $2 $5
  target=$5
  echo "CREATE SSL CONFIG... $target"
  sh -c "echo ' '                                          >> $target"
  sh -c "echo '<VirtualHost $1:443>'                       >> $target"
  sh -c "echo '    SSLEngine On'                           >> $target"
  sh -c "echo '    SSLProtocol all -SSLv2 -SSLv3'          >> $target"
  sh -c "echo '    SSLCipherSuite HIGH:MEDIUM:!aNULL:!MD5' >> $target"
  sh -c "echo '    SSLCertificateFile $3'                  >> $target"
  sh -c "echo '    SSLCertificateKeyFile $4'               >> $target"
  sh -c "echo ' '                                          >> $target"
  sh -c "echo '    DocumentRoot /usr/local/vufind2/public' >> $target"
  sh -c "echo '    ServerName $2'                          >> $target"
  sh -c "echo '    ServerAlias $2'                         >> $target"
  sh -c "echo '</VirtualHost>'                             >> $target"
}

#$1: Activate SSL (BOOL)
#$2: Module names (comma-seperated: TueLib,ixTheo)
#$3: target
createHttpdConfig() {
  target=$3
  echo "CREATE HTTPD CONFIG... $target"
  sh -c "echo '# Configuration for theme-specific resources:'                                                          > $target"
  sh -c "echo 'AliasMatch ^/themes/([0-9a-zA-Z-_]*)/css/(.*)\$ /usr/local/vufind2/themes/\$1/css/\$2'                 >> $target"
  sh -c "echo 'AliasMatch ^/themes/([0-9a-zA-Z-_]*)/images/(.*)$ /usr/local/vufind2/themes/\$1/images/\$2'            >> $target"
  sh -c "echo 'AliasMatch ^/themes/([0-9a-zA-Z-_]*)/js/(.*)\$ /usr/local/vufind2/themes/\$1/js/\$2'                   >> $target"
  sh -c "echo '<Directory ~ \"^/usr/local/vufind2/themes/([0-9a-zA-Z-_]*)/(css|images|js)/\">'                        >> $target"
  sh -c "echo '  <IfModule !mod_authz_core.c>'                                                                        >> $target"
  sh -c "echo '    Order allow,deny'                                                                                  >> $target"
  sh -c "echo '    Allow from all'                                                                                    >> $target"
  sh -c "echo '  </IfModule>'                                                                                         >> $target"
  sh -c "echo '  <IfModule mod_authz_core.c>'                                                                         >> $target"
  sh -c "echo '    Require all granted'                                                                               >> $target"
  sh -c "echo '  </IfModule>'                                                                                         >> $target"
  sh -c "echo '  AllowOverride All'                                                                                   >> $target"
  sh -c "echo '</Directory>'                                                                                          >> $target"
  sh -c "echo ''                                                                                                      >> $target"
  sh -c "echo '# Configuration for general VuFind base:'                                                              >> $target"
  sh -c "echo '<Directory /usr/local/vufind2/public/>'                                                                >> $target"
  sh -c "echo '  <IfModule !mod_authz_core.c>'                                                                        >> $target"
  sh -c "echo '    Order allow,deny'                                                                                  >> $target"
  sh -c "echo '    Allow from all'                                                                                    >> $target"
  sh -c "echo '  </IfModule>'                                                                                         >> $target"
  sh -c "echo '  <IfModule mod_authz_core.c>'                                                                         >> $target"
  sh -c "echo '    Require all granted'                                                                               >> $target"
  sh -c "echo '  </IfModule>'                                                                                         >> $target"
  sh -c "echo '  AllowOverride All'                                                                                   >> $target"
  sh -c "echo ' '                                                                                                     >> $target"
  sh -c "echo '  RewriteEngine On'                                                                                    >> $target"
  sh -c "echo '  RewriteBase /'                                                                                       >> $target"
  sh -c "echo '  '                                                                                                    >> $target"
  if [ "$1" = true ]; then
    sh -c "echo '  # Ensure that we are under SSL:'                                                                   >> $target"
    sh -c "echo '  RewriteCond   %{SERVER_PORT} !^443\$'                                                              >> $target"
    sh -c "echo '  RewriteRule   ^.*\$     https://%{SERVER_NAME}%{REQUEST_URI}  [L,R]'                               >> $target"
    sh -c "echo ' '                                                                                                   >> $target"
  fi
  sh -c "echo '  RewriteCond %{REQUEST_FILENAME} -s [OR]'                                                             >> $target"
  sh -c "echo '  RewriteCond %{REQUEST_FILENAME} -l [OR]'                                                             >> $target"
  sh -c "echo '  RewriteCond %{REQUEST_FILENAME} -d'                                                                  >> $target"
  sh -c "echo '  RewriteRule ^.*\$ - [NC,L]'                                                                          >> $target"
  sh -c "echo '  RewriteRule ^.*\$ index.php [NC,L]'                                                                  >> $target"
  sh -c "echo ' '                                                                                                     >> $target"
  sh -c "echo '  php_value short_open_tag On'                                                                         >> $target"
  sh -c "echo ' '                                                                                                     >> $target"
  sh -c "echo '  # Uncomment this line to put VuFind into development mode in order to see more detailed messages:'   >> $target"
  sh -c "echo '  #SetEnv VUFIND_ENV development'                                                                      >> $target"
  sh -c "echo ' '                                                                                                     >> $target"
  sh -c "echo '  # This line points to the local override directory where you should place your customized files'     >> $target"
  sh -c "echo '  # to override VuFind core features/settings.  Set to blank string ("") to disable.'                  >> $target"
  sh -c "echo '  SetEnv VUFIND_LOCAL_DIR /usr/local/vufind2/local'                                                    >> $target"
  sh -c "echo ' '                                                                                                     >> $target"
  sh -c "echo '  # This line specifies additional Zend Framework 2 modules to load after the standard VuFind module.' >> $target"
  sh -c "echo '  # Multiple modules may be specified separated by commas.  This mechanism can be used to override'    >> $target"
  sh -c "echo '  # core VuFind functionality without modifying core code.'                                            >> $target"
  sh -c "echo '  SetEnv VUFIND_LOCAL_MODULES $2'                                                                      >> $target"
  sh -c "echo '</Directory>'                                                                                          >> $target"
}

linkHttpdConfigFiles() {
  echo "LINK FILES..."
  # Link Files
  sudo ln -sf "/usr/local/vufind2/local/httpd-vufind.conf"        "/etc/apache2/conf-enabled/vufind2.conf"
  sudo ln -sf "/usr/local/vufind2/local/httpd-vufind-vhosts.conf" "/etc/apache2/conf-enabled/vufind2-vhosts.conf"
}


# $1: LOCAL_COPY_DIRECTORY
# $1: TARGET_FILE
createStartScript() {
  target=$2
  echo "CREATE START SCRIPT... $target"
  sh -c "> $target"
  # paths
  sh -c "echo export JAVA_HOME=\"/usr/lib/jvm/default-java\"        >> $target"
  sh -c "echo export VUFIND_HOME=\"/usr/local/vufind2\"             >> $target"
  sh -c "echo export VUFIND_LOCAL_DIR=\"/usr/local/vufind2/local\"  >> $target"
  # stop server
  sh -c "echo '/usr/local/vufind2/vufind.sh stop'                   >> $target"
  sh -c "echo 'sudo apache2ctl stop'                                >> $target"
  sh -c "echo 'sleep 5'                                             >> $target"
  # link default location
  # TODO: pwd
  sh -c "echo sudo ln -sfT $LOCAL_COPY_DIRECTORY /usr/local/vufind2 >> $target"
  # start server
  sh -c "echo 'sudo apache2ctl start'                               >> $target"
  sh -c "echo '/usr/local/vufind2/vufind.sh start'                  >> $target"
  sudo chmod ug+x $target
}

# $1: LOCAL_COPY_DIRECTORY
# $2: SERVER_URL
# $3: HTPASSWD
createHtaccessForAuthentification() {
  echo "CREATE HTACCESS FOR AUTHENTIFICATION... $1/public/.htaccess"
  sh -c "echo 'AuthType Basic'      > $1/public/.htaccess"
  sh -c "echo 'AuthName \"$2\"'    >> $1/public/.htaccess"
  sh -c "echo 'AuthUserFile $3'    >> $1/public/.htaccess"
  sh -c "echo 'Require valid-user' >> $1/public/.htaccess"
}

#$1: LOCAL_COPY_DIRECTORY
deleteHtaccessForAuthentification() {
  echo "DELETE HTACCESS FOR AUTHENTIFICATION... $1/public/.htaccess"
  rm -f $1/public/.htaccess
}

#$1: Start Script
startServer() {
  echo "START SERVER..."
  sudo sh -c "$1"
}

#$1: LOCAL_COPY_DIRECTORY
#$2: FILE
importMark() {
  echo "IMPORT MARK..."
  $1/import-marc.sh $2
}

install=false
repository=""
norm=""
uber=""
htpasswd=""
ssl_cert=
ssl_key=
force_https=false

while :; do
    case $1 in
        -h|-\?|--help) # Call a "show_help" function to display a synopsis, then exit.
            show_help
            exit
            ;;
        -a|--auth)
            if [ -n "$2" ]; then
                htpasswd=$2
                shift 2
                continue
            else
                echo "ERROR: '$1' requires a non-empty option argument." >&2
                exit 1
            fi
            ;;
        -n|--norm)
            if [ -n "$2" ]; then
                norm=$2
                shift 2
                continue
            else
                echo "ERROR: '$1' requires a non-empty option argument." >&2
                exit 1
            fi
            ;;
        -ü|--über)
            if [ -n "$2" ]; then
                uber=$2
                shift 2
                continue
            else
                echo "ERROR: '$1' requires a non-empty option argument." >&2
                exit 1
            fi
            ;;
        -i|--install)
            installDependencies
            ;;
        -c|--clone)
            if [ -n "$2" ]; then
                repository=$2
                shift 2
                continue
            else
                echo "ERROR: '$1' requires a non-empty option argument." >&2
                exit 1
            fi
            ;;
         -s|--ssl)
            if [ -n "$2" ] && [ -n "$3" ]; then
                ssl_cert=$2
                ssl_key=$3
                shift 3
                continue
            else
                echo "ERROR: '$1' requires two non-empty option argument." >&2
                exit 1
            fi
            ;;
        -fs|--force-https)
            force_https=true
            shift
            break
            ;;
        --) # End of all options.
            shift
            break
            ;;
        -?*)
            printf 'ERROR: Unknown option: %s\n' "$1" >&2
            exit 1
            ;;
        *) # Default case: If no more options then break out of the loop.
            break
    esac

    shift
done

if [ "$#" -ne 4 ]; then
  echo "ERROR: Illegal number of parameters!"
  echo "$*"
  show_help
  exit 1
fi


NAME=$1
SERVER_IP=$2
SERVER_URL=$3
MODULES=$4

export VUFIND_HOME="/usr/local/vufind2"
export VUFIND_LOCAL_DIR="$VUFIND_HOME/local"
LOCAL_COPY_DIRECTORY="$(pwd)/$NAME"
LOCAL_COPY_DIRECTORY_LOCAL_DIR="$LOCAL_COPY_DIRECTORY/local"
START_SCRIPT="$LOCAL_COPY_DIRECTORY/start-vufind.sh"
SITE_CONFIG_FILE="$LOCAL_COPY_DIRECTORY_LOCAL_DIR/config/vufind/local_overrides/site.conf"
DATABASE_CONFIG_FILE="$LOCAL_COPY_DIRECTORY_LOCAL_DIR/config/vufind/local_overrides/database.conf"
HTTPD_CONFIG_FILE="$LOCAL_COPY_DIRECTORY_LOCAL_DIR/httpd-vufind.conf"
VHOST_CONFIG_FILE="$LOCAL_COPY_DIRECTORY_LOCAL_DIR/httpd-vufind-vhosts.conf"

if [ -n "$repository" ]; then
  clone $repository $LOCAL_COPY_DIRECTORY
fi 

echo $LOCAL_COPY_DIRECTORY

sudo ln -sfT $LOCAL_COPY_DIRECTORY /usr/local/vufind2

setPrivileges $VUFIND_HOME $VUFIND_LOCAL_DIR
createEtcProfile
createServerConfig $SERVER_URL $NAME $SITE_CONFIG_FILE $DATABASE_CONFIG_FILE
createHttpdConfig $force_https $MODULES $HTTPD_CONFIG_FILE
ElinkHttpdConfigFiles
createStartScript $LOCAL_COPY_DIRECTORY $START_SCRIPT

# Use SSL
if [ -z "$ssl_cert"] || [ -z "$ssl_key"]; then 
  createVirtualHost $SERVER_IP $SERVER_URL $VHOST_CONFIG_FILE
else
  createVirtualHostWithSSH $SERVER_IP $SERVER_URL $ssl_cert $ssl_key $VHOST_CONFIG_FILE
fi

# Use Password protection
if [ -n "$htpasswd" ]; then
  createHtaccessForAuthentification $LOCAL_COPY_DIRECTORY $SERVER_URL $htpasswd
else 
  deleteHtaccessForAuthentification $LOCAL_COPY_DIRECTORY
fi

# Import mark data
if [ -n "$norm" ] || [ -n "$uber" ]; then
	startServer $START_SCRIPT
  sleep 5
  if [ -n "$uber" ]; then
    importMark $VUFIND_HOME "$uber"
  fi
  sleep 5
  if [ -n "$norm" ]; then
     importMark $VUFIND_HOME "$norm"
  fi
fi