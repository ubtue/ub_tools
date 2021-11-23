#!/bin/bash
CENTOS_TARGET_CERTIFICATE_FILE="/etc/pki/ca-trust/source/anchors/eguzkilore.crt"
UBUNTU_TARGET_CERTIFICATE_FILE="/usr/share/ca-certificates/custom/eguzkilore.crt"
if [ -f /etc/redhat-release ]; then
    if [ ! -f ${CENTOS_TARGET_CERTIFICATE_FILE} ]; then
        cp /usr/local/ub_tools/docker/zts/extra_certs/extra_certs.pem /etc/pki/ca-trust/source/anchors/eguzkilore.crt
        update-ca-trust extract
    fi
else # Hopefully we're on Ubuntu!
    if [ ! -f ${UBUNTU_TARGET_CERTIFICATE_FILE} ]; then
        cp /usr/local/ub_tools/docker/zts/extra_certs/extra_certs.pem /usr/share/ca-certificates/custom/eguzkilore.crt
    fi
    grep --quiet --extended-regexp --line-regexp  '[!]?custom[/]eguzkilore.crt' /etc/ca-certificates.conf \
        || echo 'custom/eguzkilore.crt' >> /etc/ca-certificates.conf
    dpkg-reconfigure --frontend=noninteractive ca-certificates
fi
