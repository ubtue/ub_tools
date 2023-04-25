#!/bin/bash
###########################################
# Author: Steven Lolong (steven.lolong@uni-tuebingen.de)
# copyright 2023 Tübingen University Library.  All rights reserved.
# 
# Script to generate a ixtheo_remid file
###########################################

set -o errexit -o nounset

# related to creating temporary file
declare -r FILE_ORI="/tmp/remid.xml"
declare -r FILE_TEMP="/tmp/remid_tmp.xml"
declare -r FILE_TEMP_CLEAN="/tmp/remid_tmp_clean.xml"
declare -r GET_TOTAL_RECORDS="/tmp/total_rec.xml"

# the output file
declare -r FILE_NEW="ixtheo_remid_`date +'%y%m%d'`_001.xml"
declare -r FILE_REMOVED_SERIAL="ixtheo_removed_serial_`date +'%y%m%d'`_001.txt"

# related to file name pattern
declare -r TEMP_FILE_NAME_PREFIX="tmp_file"
declare -r CLEAN_FILE_NAME_PREFIX="clean_file"

declare -r URL_PART_1="http://sru.hebis.de/sru/DB=2.1?query=pica.abr+%3D+%22REMID%22+or+pica.prv+%3D+%22REMID%22&version=1.1&operation=searchRetrieve&recordSchema=marc21&maximumRecords="
declare -r URL_PART_2="&startRecord="
declare -r URL_PART_3="&recordPacking=xml&sortKeys=LST_Y%2Cpica%2C0%2C%2C"

# maximum number of records per download 
declare -r RECORDS_PER_CALL=500


function ColorEcho {
    echo -e "\033[1;34m" $1 "\033[0m"
}


ColorEcho "========= REMID Script ===========\n"
ColorEcho "++++ Counting the total number of record to download ++++"
# the total number of records
wget "${URL_PART_1}1${URL_PART_2}1${URL_PART_3}" -O $GET_TOTAL_RECORDS


declare -r TOTAL_RECORD=$(sed -n '/srw:numberOfRecords/{s/.*<srw:numberOfRecords>\(.*\)<\/srw:numberOfRecords>.*/\1/;p}' <<< tmp_total_rec $GET_TOTAL_RECORDS)

rm $GET_TOTAL_RECORDS
## End Get the total number of records

declare -r NUMBER_OF_ITERATION=$(( (TOTAL_RECORD + RECORDS_PER_CALL - 1) / RECORDS_PER_CALL ))

ColorEcho "++++ Total record found ${TOTAL_RECORD}, and it will split into ${NUMBER_OF_ITERATION} file(s) ++++"

Counter=1
TempFile=""
CleanFile=""
StartRecord=0

echo '<?xml version="1.0" encoding="UTF-8" ?>' > $FILE_ORI
while [ $Counter -le $NUMBER_OF_ITERATION ]
do  
    ColorEcho "++++ Downloading file ${Counter} of ${NUMBER_OF_ITERATION} ++++"

    TempFile="/tmp/${TEMP_FILE_NAME_PREFIX}_${Counter}.xml"
    CleanFile="/tmp/${CLEAN_FILE_NAME_PREFIX}_${Counter}.xml"

    StartRecord=$(( ( Counter - 1) * RECORDS_PER_CALL + 1 ))
    ColorEcho "Downloading start from record ${StartRecord}"
    # download some records into the file
    wget "${URL_PART_1}${RECORDS_PER_CALL}${URL_PART_2}${StartRecord}${URL_PART_3}" -O $TempFile

    ColorEcho "Purging the file..."
    # clean the temp file
    grep -vE '<?xml\ version|xml-stylesheet|srw:|diag:|:dc|:diag|:xcql|<startRecord|</startRecord|<maximumRecords|</maximumRecords|<sortKeys|</sortKeys|<rob:|</rob:' $TempFile > $CleanFile  

    ColorEcho "Merge the clean file \n"
    cat $CleanFile >> $FILE_ORI

    # delete temp file
    rm $TempFile

    # delete the clean file
    rm $CleanFile

    (( Counter++ ))
done


ColorEcho "++++ Make the file readable by marc tool ++++"
# append this text into line 2
sed -i '1 a <collection xmlns="http://www.loc.gov/MARC21/slim" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd">' $FILE_ORI

# append this text at the end of the line
echo '</collection>' >> $FILE_ORI

# Delete empty line
sed -i '/^[[:space:]]*$/d' $FILE_ORI

ColorEcho "++++ Removing duplication in field ++++"
# remove tag(s) to avoid duplication of field with the same sub-field content and value
marc_filter $FILE_ORI $FILE_TEMP_CLEAN --remove-fields '852:  \x1FaDE-Tue135' \
    --remove-fields '084:  \x1Fa0\x1F2ssgn' \
    --remove-fields '935:  \x1Faremi\x1F2LOK'

ColorEcho "++++ Inserting field into the file ++++"
# add new tag(s) to remid
marc_augmentor $FILE_TEMP_CLEAN $FILE_TEMP  --insert-field '852:  \x1FaDE-Tue135' \
    --insert-field '084:  \x1Fa0\x1F2ssgn' \
    --insert-field '935:  \x1Faremi\x1F2LOK'

ColorEcho "++++ Copying into the ouput file ++++"
# copy field
remid_augment $FILE_TEMP $FILE_NEW $FILE_REMOVED_SERIAL

ColorEcho "++++ Remove temporary files ++++"
# remove unnecessary file
rm -f $FILE_TEMP
rm -f $FILE_TEMP_CLEAN
rm -f $FILE_ORI

ColorEcho "++++ The output filename: ${FILE_NEW} ++++"
ColorEcho "++++ The output filename: ${FILE_REMOVED_SERIAL} ++++"
ColorEcho "========== Congratulation! =========="