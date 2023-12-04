#!/bin/bash
set -o errexit

# install paramiko library as a dependency of sftp

apt-get --yes install python3-paramiko
