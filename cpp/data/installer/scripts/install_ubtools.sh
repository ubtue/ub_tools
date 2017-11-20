#!/bin/bash
#
# Install minimum prerequisites (including git)
# download ub_tools (via git)
# compile cpp installer
# run it

# detect OS
if [ -e /etc/debian_version ]; then
	# ubuntu
	echo "Ubuntu detected! installing dependencies..."
        cd /tmp
        if [ ! -e ./install_ubuntu_packages.sh ]; then
            apt-get --yes update
            apt-get --yes install curl
            curl https://raw.githubusercontent.com/ubtue/ub_tools/master/cpp/data/installer/scripts/install_ubuntu_packages.sh -o ./install_ubuntu_packages.sh
            chmod 700 ./install_ubuntu_packages.sh
        fi
	./install_ubuntu_packages.sh
elif [ -e /etc/redhat-release ]; then
	# centos
	echo "CentOS detected! installing dependencies..."
        cd /tmp
        if [ ! -e ./install_centos_packages.sh ]; then
            yum -y update
            yum -y install curl
            curl https://raw.githubusercontent.com/ubtue/ub_tools/master/cpp/data/installer/scripts/install_centos_packages.sh -o ./install_centos_packages.sh
            chmod 700 ./install_centos_packages.sh
        fi
	./install_centos_packages.sh
else
	echo "OS type could not be detected! aborting"
	exit 1
fi

if [ -d /usr/local/ub_tools ]; then
	echo "ub_tools already exists, skipping download"
else
	echo "cloning ub_tools"
	git clone https://github.com/ubtue/ub_tools.git /usr/local/ub_tools
fi

echo "building prerequisites"
cd /usr/local/ub_tools/cpp/lib/mkdep && CCC=clang++ make install

echo "building cpp installer"
cd /usr/local/ub_tools/cpp && CCC=clang++ make installer

echo "starting cpp installer"
/usr/local/ub_tools/cpp/installer $*
