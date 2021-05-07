FROM salrashid123/squidproxy

# SQUID config
COPY ./squid.conf /apps/squid.conf.https_proxy
COPY ./proxyCA.pem /apps/ssl_certs/proxyCA.pem
COPY ./entrypoint.sh /sbin/entrypoint.sh
COPY ./squid_localnet.conf /apps/squid_localnet.conf

# run the application
EXPOSE 3128
ENTRYPOINT [ "/sbin/entrypoint.sh" ]
