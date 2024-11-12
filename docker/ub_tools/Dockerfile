FROM ub_tools-deps
LABEL maintainer="Mario Trojan <mario.trojan@uni-tuebingen.de>"

# add needed configuration files
# (+simulate mounted directories)
COPY mnt/ZE020110 /mnt/ZE020110
COPY mnt/ZE020150 /mnt/ZE020150
COPY .dockerenv /

# download & run ubtools installation script
# (use curl because it's part of standard centos installation)
ENV BRANCH=slolong_tuefind91
RUN curl https://raw.githubusercontent.com/ubtue/ub_tools/master/cpp/data/installer/scripts/install_ubtools.sh -o /tmp/install_ubtools.sh \
    && chmod 700 /tmp/install_ubtools.sh
RUN /tmp/install_ubtools.sh ub-tools-only

# configure apache
EXPOSE 80
EXPOSE 443
COPY apache2/*.conf /etc/httpd/conf.d/
COPY apache2/*.pem /etc/ssl/certs/

# start services when container is run
COPY startup.sh /startup.sh
CMD [ "/startup.sh" ]
