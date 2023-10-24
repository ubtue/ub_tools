FROM ubuntu:jammy
LABEL maintainer="Mario Trojan <mario.trojan@uni-tuebingen.de>"

# Set non-interactive mode for UBUNTU to avoid interruptions
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# set java encoding to avoid problems when compiling solr plugins
ENV JAVA_TOOL_OPTIONS="-Dfile.encoding=UTF8"

# download & run ubtools installation script
# (neither curl nor wget is pre-installed in docker container)
RUN apt-get --yes update && apt-get --yes install curl
RUN curl https://raw.githubusercontent.com/ubtue/ub_tools/master/cpp/data/installer/scripts/install_ubuntu_packages.sh -o /tmp/install_ubuntu_packages.sh \
    && chmod 700 /tmp/install_ubuntu_packages.sh
RUN /tmp/install_ubuntu_packages.sh
