FROM salrashid123/squidproxy

# SQUID config
ADD ./squid.conf /apps/squid.conf.https_proxy
ADD ./proxyCA.pem /apps/ssl_certs/proxyCA.pem
add ./entrypoint.sh /sbin/entrypoint.sh

# run the application
EXPOSE 3128
ENTRYPOINT [ "/sbin/entrypoint.sh" ]
