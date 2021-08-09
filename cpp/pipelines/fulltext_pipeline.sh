#!/bin/bash
set -o errexit -o nounset

source pipeline_functions.sh

TMP_NULL="/tmp/null"
function CreateTemporaryNullDevice {
    if [ ! -c ${TMP_NULL} ]; then
        mknod ${TMP_NULL} c 1 3
        chmod 666 ${TMP_NULL}
    fi
}

if [ $# != 1 ]; then
    echo "usage: $0 GesamtTiteldaten-YYMMDD.mrc"
    exit 1
fi

if [[ ! "$1" =~ GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc ]]; then
    echo 'Die Gesamttiteldatendatei entspricht nicht dem Muster GesamtTiteldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc!'
    exit 1
fi

# Determines the embedded date of the files we're processing:
date=$(DetermineDateFromFilename $1)

# Sets up the log file:
logdir=/usr/local/var/log/tuefind
log="${logdir}/fulltext_pipeline.log"
rm -f "${log}"

CleanUp
CreateTemporaryNullDevice

OVERALL_START=$(date +%s.%N)

StartPhase "Create Match DB"
(create_match_db GesamtTiteldaten-"${date}".mrc
    >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait


StartPhase "Import Mohr Data"
(find /usr/local/publisher_fulltexts/mohr/ -maxdepth 1 -name '*.txt' | xargs -n 50 store_in_elasticsearch --set-publisher-provided \
    >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Import Brill Data"
(find /usr/local/publisher_fulltexts/brill/ -maxdepth 1 -name '*.txt' | xargs -n 50 store_in_elasticsearch --set-publisher-provided \
    >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Import Aschendorff Data"
(find /usr/local/publisher_fulltexts/aschendorff/ -maxdepth 1 -name '*.txt' | xargs -n 50 store_in_elasticsearch --set-publisher-provided \
    >> "${log}" 2>&1 && \
EndPhase || Abort) &


StartPhase "Harvest Title Data Fulltext"
(create_full_text_db --store-pdfs-as-html --use-separate-entries-per-url --include-all-tocs \
    --include-list-of-references --only-pdf-fulltexts GesamtTiteldaten-"${date}".mrc ${TMP_NULL} \
    >> "${log}" 2>&1 && \
        EndPhase || Abort) &
wait


echo -e "\n\nPipeline done after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes." | tee --append "${log}"
echo "*** FULL TEXT PIPELINE DONE - $(date) ***" | tee --append "${log}"




