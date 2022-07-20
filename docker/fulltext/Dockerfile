FROM ubuntu:focal
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

COPY elasticsearch.yml /etc/elasticsearch/elasticsearch.yml
COPY Elasticsearch.conf /usr/local/var/lib/tuelib/Elasticsearch.conf

# set java encoding to avoid problems when compiling solr plugins
ENV JAVA_TOOL_OPTIONS="-Dfile.encoding=UTF8"

# download & run ubtools installation script
# (neither curl nor wget is pre-installed in docker container)
RUN apt-get --yes update && apt-get --yes install curl
RUN curl "https://raw.githubusercontent.com/ubtue/ub_tools/{BRANCH}/cpp/data/installer/scripts/install_ubtools.sh" -o /tmp/install_ubtools.sh \
    && chmod 700 /tmp/install_ubtools.sh
RUN /tmp/install_ubtools.sh fulltext-backend --test --omit-cronjobs --omit-systemctl

# configure ES
EXPOSE 9200

# start services when container is run
COPY startup.sh /startup.sh
CMD [ "/startup.sh" ]
