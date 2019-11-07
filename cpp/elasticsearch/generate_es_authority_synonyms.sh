#!/bin/bash

STAGE_FILE="stage_file"
STAGE_TMP_FILE="stage_tmp_file"
OUTPUT_FILE="synonyms.txt"

> ${OUTPUT_FILE}
> ${STAGE_FILE}

# Generate a merged file form the extracted translation files
for translation_file in normdata_translations_*.txt
do
     join -t '|' -j 1 -a 1 -a 2  --nocheck-order <(sort ${STAGE_FILE})  <(sort ${translation_file}) > ${STAGE_TMP_FILE}
     mv ${STAGE_TMP_FILE} ${STAGE_FILE}
done

#Escape commas in place
sed --in-place --expression 's/,/\\,/g' ${STAGE_FILE}
#Replace our separators '|' and '||'
sed -r --in-place --expression 's/[|]+/, /g' ${STAGE_FILE}

mv ${STAGE_FILE} ${OUTPUT_FILE}
