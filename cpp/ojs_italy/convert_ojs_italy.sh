#!/bin/bash
# Convert data from OJS Italy
# For Studia patavina identify records that are already present in IxTheo and remove them from the final file

function RemoveTempFiles {
   for tmpfile in ${tmpfiles[@]}; do
       rm ${tmpfile}
   done
}

trap RemoveTempFiles EXIT



tmpfiles=()
tmp_stdout="/tmp/stdout.xml"
ln --symbolic /dev/stdout ${tmp_stdout}
tmpfiles+=(${tmp_stdout})


STUDIA_PATAVINA_ORIG_IN="daten/studia_patavina_complete/studia_patavina_complete.xml"
RIVISTA_DI_SCIENCE_ORIG_IN='daten/complete_all.mrc'
date=$(date '+%y%m%d')
STUDIA_PATAVINA_OUT="daten/studia_patavina_complete/studia_patavina_complete_filtered_ubtue_${date}.xml"
STUDIA_PATAVINA_FULL="${STUDIA_PATAVINA_OUT/_filtered/}"
RIVISTA_DI_SCIENCE_OUT="daten/complete_all_converted_${date}.xml"
SAMPLE_LIMIT="5"
TEST_DB_SAMPLE_DIR="daten/test_db_sample"
SAMPLE_BASE_NAME="ixtheo_ojsitaly_complete"
STUDIA_PATAVINA_ASSOCIATED_AUTHORS="daten/studia_patavina_associated_authors.txt"
RIVISTA_DI_SCIENCE_ASSOCIATED_AUTHORS="daten/rivista_di_science_associated_authors.txt"


if [ $# != 1 ]; then
    echo "Usage: $0 ixtheo_solr_host_and_port"
    exit 1
fi

solr_host_and_port="$1"

./convert_ojs_italy ${STUDIA_PATAVINA_ORIG_IN} studia_patavina_map.txt \
                    ${STUDIA_PATAVINA_OUT}
./convert_ojs_italy ${RIVISTA_DI_SCIENCE_ORIG_IN} rivista_di_science_map.txt \
                    ${RIVISTA_DI_SCIENCE_OUT}

#Some Cleanup
marc_filter ${RIVISTA_DI_SCIENCE_OUT} ${tmp_stdout} --globally-substitute '100d' '[.]$' '' | sponge  ${RIVISTA_DI_SCIENCE_OUT}
marc_filter ${RIVISTA_DI_SCIENCE_OUT} ${tmp_stdout} --globally-substitute '100a' '[.]$' '' | sponge  ${RIVISTA_DI_SCIENCE_OUT}
marc_filter ${RIVISTA_DI_SCIENCE_OUT} ${tmp_stdout} --globally-substitute '100a' '[,]$' '' | sponge  ${RIVISTA_DI_SCIENCE_OUT}
marc_filter ${RIVISTA_DI_SCIENCE_OUT} ${tmp_stdout} --globally-substitute '700a' '[.]$' '' | sponge  ${RIVISTA_DI_SCIENCE_OUT}
marc_filter ${RIVISTA_DI_SCIENCE_OUT} ${tmp_stdout} --globally-substitute '700a' '[,]$' '' | sponge  ${RIVISTA_DI_SCIENCE_OUT}

#Associate authors

if [ ! -f ${STUDIA_PATAVINA_ASSOCIATED_AUTHORS} ]; then
    marc_grep ${STUDIA_PATAVINA_OUT} '"100a"' no_label | \
        xargs -n 1 -I'{}' sh -c 'echo "$@" "'"|"'" $(./swb_author_lookup --sloppy-filter "$@")' _ '{}' \
        | sort | uniq > ${STUDIA_PATAVINA_ASSOCIATED_AUTHORS}
fi

if [ ! -f ${RIVISTA_DI_SCIENCE_ASSOCIATED_AUTHORS} ]; then
    marc_grep ${RIVISTA_DI_SCIENCE_OUT} '"100a"' no_label | sort | uniq | tr "'" '’' | \
        xargs -n 1 -I'{}' sh -c 'echo "$@" "'"|"'" $(./swb_author_lookup --sloppy-filter "$@")' _ '{}' \
        > ${RIVISTA_DI_SCIENCE_ASSOCIATED_AUTHORS}
fi

./add_author_associations ${STUDIA_PATAVINA_OUT} /tmp/stdout.xml \
    <(cat ${STUDIA_PATAVINA_ASSOCIATED_AUTHORS}  | grep -P -v '[|]\s*$') \
    | sponge ${STUDIA_PATAVINA_OUT}
./add_author_associations  ${RIVISTA_DI_SCIENCE_OUT} /tmp/stdout.xml \
     <(cat ${RIVISTA_DI_SCIENCE_ASSOCIATED_AUTHORS} | grep -P -v '[|]\s*$') \
     | sponge ${RIVISTA_DI_SCIENCE_OUT}

# Fix Popes for RSE
john_paul='Iohannes Paulus PP$'
marc_augmentor ${RIVISTA_DI_SCIENCE_OUT} /tmp/stdout.xml \
     --add-subfield-if '1000:(DE-588)118558064' '100a:'"${john_paul}" \
     --add-subfield-if '100c:Papst' '100a:'"${john_paul}" \
     --add-subfield-if '100b:II.' '100a:'"${john_paul}" \
     --replace-subfield-if-regex '100a:/.*PP$/Johannes Paul/' '100a:'"${john_paul}" \
    | sponge ${RIVISTA_DI_SCIENCE_OUT}

benedict='Benedictus PP$'
marc_augmentor ${RIVISTA_DI_SCIENCE_OUT} /tmp/stdout.xml \
     --add-subfield-if '1000:(DE-588)118598546' '100a:'"${benedict}" \
     --add-subfield-if '100c:Papst' '100a:'"${benedict}" \
     --add-subfield-if '100b:XVI.' '100a:'"${benedict}" \
     --replace-subfield-if-regex '100a:/.*PP$/Benedikt/' '100a:'"${benedict}" \
    | sponge ${RIVISTA_DI_SCIENCE_OUT}

# Remove clearly associable records that are already present in IxTheo
cp --archive --verbose ${STUDIA_PATAVINA_OUT} ${STUDIA_PATAVINA_FULL}
marc_grep ${STUDIA_PATAVINA_FULL} \
    'if "001"!="('$(./detect_existing_studia_patavina_records.sh ${solr_host_and_port} \
         ${STUDIA_PATAVINA_FULL} | paste --serial --delimiter='|')')" extract *' \
    marc_xml > /tmp/stdout.xml \
    | sponge ${STUDIA_PATAVINA_OUT}


# Generate txt representation
marc_grep ${STUDIA_PATAVINA_OUT} 'if "001"==".*" extract *' traditional \
          > ${STUDIA_PATAVINA_OUT%.xml}.txt

marc_grep ${RIVISTA_DI_SCIENCE_OUT} 'if "001"==".*" extract *' traditional \
          > ${RIVISTA_DI_SCIENCE_OUT%.xml}.txt

#Generate Samples
marc_convert --limit ${SAMPLE_LIMIT} ${STUDIA_PATAVINA_OUT} ${TEST_DB_SAMPLE_DIR}/${SAMPLE_BASE_NAME}_${date}_001.xml
marc_convert --limit ${SAMPLE_LIMIT} ${RIVISTA_DI_SCIENCE_OUT} ${TEST_DB_SAMPLE_DIR}/${SAMPLE_BASE_NAME}_${date}_002.xml
