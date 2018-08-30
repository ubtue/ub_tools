FROM centos:latest
MAINTAINER Mario Trojan <mario.trojan@uni-tuebingen.de>

# add needed configuration files
# (+simulate mounted /mnt/ZE020150)
ADD mnt/ZE020150 /mnt/ZE020150

# set java encoding to avoid problems when compiling solr plugins
ENV JAVA_TOOL_OPTIONS="-Dfile.encoding=UTF8"

# download & run ubtools installation script
# (use curl because it's part of standard centos installation)
RUN curl https://raw.githubusercontent.com/ubtue/ub_tools/master/cpp/data/installer/scripts/install_ubtools.sh -o /tmp/install_ubtools.sh \
    && chmod 700 /tmp/install_ubtools.sh
RUN /tmp/install_ubtools.sh --ub-tools-only

# configure mariadb (needs to be done via temp file)
# (see https://stackoverflow.com/questions/26982905/mysql-setup-using-docker)
EXPOSE 3306
RUN mysql_install_db --user=mysql --ldata=/var/lib/mysql/ --basedir=/usr
RUN echo "mysqld_safe &" > /tmp/config \
    && echo "mysqladmin --silent --wait=30 ping || exit 1" >> /tmp/config \
    && echo "mysql -u root -e \"CREATE DATABASE ub_tools;\"" >> /tmp/config \
    && echo "mysql -u root ub_tools < /usr/local/ub_tools/cpp/data/installer/ub_tools.sql" >> /tmp/config \
    && echo "mysql -u root -e \"CREATE USER ub_tools@localhost IDENTIFIED BY 'ub_tools';\"" >> /tmp/config \
    && echo "mysql -u root -e \"GRANT ALL ON *.* TO ub_tools@localhost;\"" >> /tmp/config \
    && echo "mysql -u root -e \"FLUSH PRIVILEGES;\"" >> /tmp/config \
    && bash /tmp/config \
    && rm -f /tmp/config

# configure apache
EXPOSE 80
EXPOSE 443
ADD httpd/*.conf /etc/httpd/conf.d/
ADD httpd/*.pem /etc/ssl/certs/

# start services when container is run
ADD startup.sh /startup.sh
CMD [ "/startup.sh" ]
