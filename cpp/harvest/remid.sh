#!/bin/bash
# This file's main purpose is documentation

FILE_ORI="remid.xml"
FILE_TEMP="remid_tmp.xml"
FILE_TEMP1="remid_tmp1.xml"
FILE_TEMP_CLEAN="remid_tmp_clean.xml"
FILE_NEW="ixtheo_remid_`date +'%y%m%d'`_001.xml"

# Download MARC XML from Hebis via SRU
# (be sure to increase the "maximumRecords" parameter if necessary, right now there are ~500 records)
wget 'http://sru.hebis.de/sru/DB=2.1?query=pica.abr+%3D+%22REMID%22+or+pica.prv+%3D+%22REMID%22&version=1.1&operation=searchRetrieve&stylesheet=http%3A%2F%2Fsru.hebis.de%2Fsru%2F%3Fxsl%3DsearchRetrieveResponse&recordSchema=marc21&maximumRecords=1000&startRecord=1&recordPacking=xml&sortKeys=LST_Y%2Cpica%2C0%2C%2C' -O $FILE_ORI

# Remove certain tags from non-MARC namespaces so we can use our tools
grep -vE 'xml-stylesheet|srw:|diag:|:dc|:diag|:xcql|<startRecord|</startRecord|<maximumRecords|</maximumRecords|<sortKeys|</sortKeys|<rob:|</rob:' $FILE_ORI > $FILE_TEMP

# Broken head+tail need to be removed by hand (including stylesheet and so on)
# also: insert <collection> root element at start/end.
sed -i '1 a <collection xmlns="http://www.loc.gov/MARC21/slim" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd">' $FILE_TEMP

echo '</collection>' >> $FILE_TEMP

# Delete empty line
sed -i '/^[[:space:]]*$/d' $FILE_TEMP

# currently waiting for @relhei to specify which metadata needs to be changed before Uploading to BSZ.

# remove tag(s) to avoid duplication of field with the same sub-field content and value
marc_filter $FILE_TEMP $FILE_TEMP_CLEAN --remove-fields '852:  \x1FaDE-Tue135' \
    --remove-fields '084:  \x1Fa0\x1F2ssgn' \
    --remove-fields '935:  \x1Faremi\x1F2LOK'

# add new tag(s) to remid
marc_augmentor $FILE_TEMP_CLEAN $FILE_TEMP1  --insert-field '852:  \x1FaDE-Tue135' \
    --insert-field '084:  \x1Fa0\x1F2ssgn' \
    --insert-field '935:  \x1Faremi\x1F2LOK'

# copy field
remid_augment $FILE_TEMP1 $FILE_NEW

# remove unnecessary file
rm -f $FILE_TEMP
rm -f $FILE_TEMP_CLEAN
rm -f $FILE_ORI