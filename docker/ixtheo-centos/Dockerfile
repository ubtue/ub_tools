FROM centos:8
MAINTAINER Mario Trojan <mario.trojan@uni-tuebingen.de>

# add needed configuration files
# (+simulate mounts)
RUN mkdir --parents /mnt/ZE020110/FID-Projekte
COPY mnt/ZE020150 /mnt/ZE020150
COPY .smbcredentials /root/.smbcredentials

# set java encoding to avoid problems when compiling solr plugins
ENV JAVA_TOOL_OPTIONS="-Dfile.encoding=UTF8"

# download & run ubtools installation script
# (use curl because it's part of standard centos installation)
RUN curl https://raw.githubusercontent.com/ubtue/ub_tools/master/cpp/data/installer/scripts/install_ubtools.sh -o /tmp/install_ubtools.sh \
    && chmod 700 /tmp/install_ubtools.sh
RUN /tmp/install_ubtools.sh vufind ixtheo --test --omit-cronjobs --omit-systemctl

# add local config
COPY local_overrides/* /usr/local/vufind/local/tuefind/local_overrides/

# expose solr & import example marc data
EXPOSE 8983
COPY data.mrc /usr/local/ub_tools/bsz_daten/data.mrc
RUN source /etc/profile.d/vufind.sh \
    && sudo -u solr /usr/local/vufind/solr.sh start \
    && /usr/local/vufind/import-marc.sh /usr/local/ub_tools/bsz_daten/data.mrc

# configure apache
EXPOSE 80
EXPOSE 443
COPY httpd/*.conf /etc/httpd/conf.d/
COPY httpd/*.conf.include /etc/httpd/conf.d/
COPY httpd/*.crt /etc/pki/tls/certs/
COPY httpd/*.key /etc/pki/tls/private/

# start services when container is run
COPY startup.sh /startup.sh
CMD [ "/startup.sh" ]
