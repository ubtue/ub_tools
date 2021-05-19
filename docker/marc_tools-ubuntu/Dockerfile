FROM ub_tools-deps-ubuntu
MAINTAINER Mario Trojan <mario.trojan@uni-tuebingen.de>

# install curl
RUN apt --quiet --yes update && apt --quiet --yes upgrade
RUN apt --quiet --yes install wget unzip

# download ub_tools ZIP package
RUN mkdir /tmp/ub_tools
RUN cd /tmp/ub_tools && wget --no-verbose https://github.com/ubtue/ub_tools/archive/master.zip
RUN cd /tmp/ub_tools && unzip /tmp/ub_tools/master.zip
RUN apt --quiet --yes install /tmp/ub_tools/ub_tools-master/cpp/marc_tools/debs/*.deb

# keep container running
ADD startup.sh /startup.sh
CMD [ "/startup.sh" ]
