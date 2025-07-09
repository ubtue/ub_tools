#!/bin/bash
#
# Install minimum prerequisites (including git)
# download ub_tools (via git)
# compile cpp installer
# run it


# make sure we are on ubuntu
if [ -e /etc/debian_version ]; then
    #print ubuntu version
    lsb_release -a
else
    echo "OS type could not be detected or is not supported! aborting"
    exit 1
fi

# check prerequisites and invariants
if [ "$(id -u)" != "0" ]; then
    echo "This script must be run as root" 1>&2
    exit 1
fi
if [[ ! $PATH =~ "/usr/local/bin" ]]; then
    echo "Please add /usr/local/bin to your PATH before starting the installation!"
    exit 1
fi

if [ -z "$BRANCH" ]; then
    BRANCH="master"
fi
echo "Branch is: ${BRANCH}"

echo "Installing dependencies..."

cd /tmp
if [ ! -e ./install_ubuntu_packages.sh ]; then
    apt-get --quiet --yes update
    apt-get --yes install curl
    curl --fail "https://raw.githubusercontent.com/ubtue/ub_tools/${BRANCH}/cpp/data/installer/scripts/install_ubuntu_packages.sh" -o ./install_ubuntu_packages.sh
    chmod 700 ./install_ubuntu_packages.sh
fi
./install_ubuntu_packages.sh

if [ -d /usr/local/ub_tools ]; then
    echo "ub_tools already exists, skipping download"
else
    echo "cloning ub_tools --branch ${BRANCH}"
    git clone --branch ${BRANCH} https://github.com/ubtue/ub_tools.git /usr/local/ub_tools
fi

echo "building prerequisites"
cd /usr/local/ub_tools/cpp/lib/mkdep && CCC=clang++ make --jobs=4 install

echo "building cpp installer"
cd /usr/local/ub_tools/cpp && CCC=clang++ make --jobs=4 installer

echo "starting cpp installer"
/usr/local/ub_tools/cpp/installer $*
