#!/bin/bash
cd /usr/local/zotero-translators/translators 
echo "<h4>Translators:</h4>"
/usr/bin/git config --get remote.origin.url
/usr/bin/git branch --show-current
echo "<hrule>"
echo "<h4>zotero-enhancement-maps:</h4>"
cd /usr/local/var/lib/tuelib/zotero-enhancement-maps
/usr/bin/git config --get remote.origin.url
/usr/bin/git branch --show-current
