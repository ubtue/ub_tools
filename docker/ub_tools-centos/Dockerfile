FROM centos:8
MAINTAINER Mario Trojan <mario.trojan@uni-tuebingen.de>

# add needed configuration files
# (+simulate mounted /mnt/ZE020150)
COPY mnt/ZE020150 /mnt/ZE020150

# set java encoding to avoid problems when compiling solr plugins
ENV JAVA_TOOL_OPTIONS="-Dfile.encoding=UTF8"

# download & run ubtools installation script
# (use curl because it's part of standard centos installation)
RUN curl https://raw.githubusercontent.com/ubtue/ub_tools/master/cpp/data/installer/scripts/install_ubtools.sh -o /tmp/install_ubtools.sh \
    && chmod 700 /tmp/install_ubtools.sh
RUN /tmp/install_ubtools.sh ub-tools-only

# configure apache
EXPOSE 80
EXPOSE 443
COPY httpd/*.conf /etc/httpd/conf.d/
COPY httpd/*.pem /etc/ssl/certs/

# start services when container is run
COPY startup.sh /startup.sh
CMD [ "/startup.sh" ]
