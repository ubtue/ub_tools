FROM ub_tools-deps
LABEL maintainer="Mario Trojan <mario.trojan@uni-tuebingen.de>"

#master is default value
ARG branch=master
ENV BRANCH=$branch

# add needed configuration files
# (+simulate mounts)
RUN mkdir --parents /mnt/ZE020110/FID-Projekte
COPY mnt/ZE020150 /mnt/ZE020150
COPY .smbcredentials /root/.smbcredentials

# Set non-interactive mode for UBUNTU to avoid interruptions
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# set java encoding to avoid problems when compiling solr plugins
ENV JAVA_TOOL_OPTIONS="-Dfile.encoding=UTF8"

# download & run ubtools installation script
# (neither curl nor wget is pre-installed in docker container)
RUN apt-get --yes update && apt-get --yes install curl
RUN curl --fail "https://raw.githubusercontent.com/ubtue/ub_tools/${BRANCH}/cpp/data/installer/scripts/install_ubtools.sh" -o /tmp/install_ubtools.sh \
    && chmod 700 /tmp/install_ubtools.sh
RUN /tmp/install_ubtools.sh vufind ixtheo --test --omit-cronjobs --omit-systemctl

# add local config
COPY local_overrides/* /usr/local/vufind/local/tuefind/local_overrides/

# expose solr & import example marc data
EXPOSE 8983
COPY biblio.mrc /usr/local/ub_tools/bsz_daten/biblio.mrc
COPY auth.mrc /usr/local/ub_tools/bsz_daten/auth.mrc
RUN . /etc/profile.d/vufind.sh \
    && sudo -u solr /usr/local/vufind/solr.sh start \
    && /usr/local/vufind/import-marc.sh /usr/local/ub_tools/bsz_daten/biblio.mrc \
    && /usr/local/vufind/import-marc-auth.sh /usr/local/ub_tools/bsz_daten/auth.mrc

# configure apache
EXPOSE 80
EXPOSE 443
RUN rm /etc/apache2/sites-enabled/000-default.conf
COPY apache2/*.conf /etc/apache2/sites-available/
RUN ln -s /etc/apache2/sites-available/vufind-vhosts.conf /etc/apache2/sites-enabled/vufind-vhosts.conf
COPY apache2/*.pem /etc/ssl/certs/

# start services when container is run
COPY startup.sh /startup.sh
CMD [ "/startup.sh" ]
