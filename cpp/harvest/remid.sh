#!/bin/bash
# This file's main purpose is documentation

# Download MARC XML from Hebis via SRU
# (be sure to increase the "maximumRecords" parameter if necessary, right now there are ~500 records)
wget 'http://sru.hebis.de/sru/DB=2.1?query=pica.abr+%3D+%22REMID%22+and+pica.tit+exact+%22Sammelmappe%22&version=1.1&operation=searchRetrieve&stylesheet=http%3A%2F%2Fsru.hebis.de%2Fsru%2F%3Fxsl%3DsearchRetrieveResponse&recordSchema=marc21&maximumRecords=10000&startRecord=1&recordPacking=xml&sortKeys=LST_tY%2Cpica%2C0%2C%2C' -O remid.xml

# Remove certain tags from non-MARC namespaces so we can use our tools
cat remid.xml | grep -v srw: | grep -v diag: > remid_clean.xml

# Broken head+tail need to be removed by hand (including stylesheet and so on)
# also: insert <collection> root element at start/end.

# currently waiting for @relhei to specify which metadata needs to be changed before Uploading to BSZ.
