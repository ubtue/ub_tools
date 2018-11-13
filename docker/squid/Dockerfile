FROM sameersbn/squid

# SQUID config
ADD ./squid.conf /etc/squid/squid.conf

# run the application
EXPOSE 3128 3129
ENTRYPOINT [ "/sbin/entrypoint.sh" ]
