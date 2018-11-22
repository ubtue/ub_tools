FROM salrashid123/squidproxy

# SQUID config
ADD ./squid.conf /apps/squid.conf.https_proxy
ADD ./proxyCA.pem /apps/ssl_certs/proxyCA.pem
ADD ./entrypoint.sh /sbin/entrypoint.sh
ADD ./squid_localnet.conf /apps/squid_localnet.conf

# run the application
EXPOSE 3128
ENTRYPOINT [ "/sbin/entrypoint.sh" ]
