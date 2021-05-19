FROM ub_tools-deps-ubuntu
MAINTAINER Mario Trojan <mario.trojan@uni-tuebingen.de>

# add needed configuration files
# (+simulate mounts)
RUN mkdir --parents /mnt/ZE020110/FID-Projekte/Default
ADD mnt/ZE020150 /mnt/ZE020150
ADD .smbcredentials /root/.smbcredentials

# Set non-interactive mode for UBUNTU to avoid interruptions
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# set java encoding to avoid problems when compiling solr plugins
ENV JAVA_TOOL_OPTIONS="-Dfile.encoding=UTF8"

# download & run ubtools installation script
# (neither curl nor wget is pre-installed in docker container)
RUN apt-get --yes update && apt-get --yes install curl
RUN curl https://raw.githubusercontent.com/ubtue/ub_tools/master/cpp/data/installer/scripts/install_ubtools.sh -o /tmp/install_ubtools.sh \
    && chmod 700 /tmp/install_ubtools.sh
RUN /tmp/install_ubtools.sh vufind krimdok --test --omit-cronjobs --omit-systemctl

# add local config
ADD local_overrides/* /usr/local/vufind/local/tuefind/local_overrides/

# build css
RUN php /usr/local/vufind/util/cssBuilder.php

# expose solr & import example marc data
EXPOSE 8983
ADD data.mrc /usr/local/ub_tools/bsz_daten/data.mrc
RUN . /etc/profile.d/vufind.sh \
    && sudo -u solr /usr/local/vufind/solr.sh start \
    && /usr/local/vufind/import-marc.sh /usr/local/ub_tools/bsz_daten/data.mrc

# configure apache
EXPOSE 80
EXPOSE 443
RUN rm /etc/apache2/sites-enabled/000-default.conf
ADD apache2/*.conf /etc/apache2/sites-available/
RUN ln -s /etc/apache2/sites-available/vufind-vhosts.conf /etc/apache2/sites-enabled/vufind-vhosts.conf
ADD apache2/*.pem /etc/ssl/certs/

# start services when container is run
ADD startup.sh /startup.sh
CMD [ "/startup.sh" ]
