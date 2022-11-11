# Pre
1. copy folder "apache2" into server "/tmp/apache2"
2. copy *.mrc file of biblio and auth into server "/tmp"
3. copy file "installUBtools_allInOne.sh" into server "/tmp"

## Server
1. Disable all firewall, because composer will use many sources from internet for installation

# Do
run:
    ``/tmp/installUBTools_allInOne.sh <vufind | ub_tools_only> <OPTION> [PARAM]``

ex. for vufind server for test machine
    ``/tmp/installUBTools_allInOne.sh vufind ixtheo --test --omit-cronjobs``

# Post
## Server
1. Enable firewall
2. PHP (PL) - Check php configuration (php.ini):
    a. max_input_vars 100000 (and for URL shortener)
    b. memory_limit 256M
3. MySQL (DB):
    a. Change the root password for localhost access:
        ``alter user 'user'@'localhost' IDENTIFIED WITH mysql_native_password BY'<passwd>';``
        ``flush privileges;``
    b. if the mysql version is MariaDB:
        ``innodb_file_per_table = ON``
4. Postfix (Mail):
5. Check swap file size
   a. Ex. for Ubuntu approvx. 2GB, should be increased to 8GB
6. Check apache file configuration