Install and run VuFind locally
==============================

To install a copy of VuFind locally, your current working directory has to be the root directory of the copy and is not allowed to be `/usr/local/vufind2`.

Prepair your system
-------------------

First you have to install all dependencies with

>     ./install-vufind.sh -i 

then you have to setup your mysql-database with a user named `vufind` and the password.

Install
-------

There are two install scripts for ixTheo and KrimDok. After executing a script there will be a ixTheo or KrimDoc dictionary with all configurations.

Update
------

There are two update scripts for ixTheo and KrimDok. After executing a script the configurations of ixTheo/KrimDoc are overridden.

Start
-----

After installing, each copy has a `start-vufind.sh` script. This script is for starting this copy of VuFind. If currently another copy is running, it'll be stopped first by the script.