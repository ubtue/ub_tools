#!/bin/bash
# Factored out run for collect_journal_stats
source pipeline_functions.sh
declare -r -i FIFO_BUFFER_SIZE=1000000 # in bytes


TMP_NULL="/tmp/null"
function CreateTemporaryNullDevice {
    if [ ! -c ${TMP_NULL} ]; then
        mknod ${TMP_NULL} c 1 3
        chmod 666 ${TMP_NULL}
    fi
}


function Usage {
    echo "usage: $0 ixtheo|krimdok"
    exit 1
}

if [ $# != 1 ]; then
    Usage
fi

system_type="$1"

if [[  ${system_type} != ixtheo && ${system_type} != krimdok ]]; then
    Usage
fi 

title_file=$(ls -1 GesamtTiteldaten-post-pipeline-* | sort --reverse | head --lines 1)

if [[ ! "${title_file}" =~ GesamtTiteldaten-post-pipeline-[0-9][0-9][0-9][0-9][0-9][0-9].mrc ]]; then
    echo 'Could not identify a file matching GesamtTiteldaten-post-pipeline-[0-9][0-9][0-9][0-9][0-9][0-9].mrc!'
    exit 1
fi


# Determines the embedded date of the files we're processing:
date=$(DetermineDateFromFilename ${title_file})

# Sets up the log file:
logdir=/usr/local/var/log/tuefind
log="${logdir}/collect_journal_stats.log"
rm -f "${log}"

CreateTemporaryNullDevice
echo -e "\n\nUsing \"${title_file}\" as input for ${system_type}" | tee --append "${log}"

OVERALL_START=$(date +%s.%N)

# Note: It is necessary to run this phase after articles have had their journal's PPN's inserted!
StartPhase "Populate the Zeder Journal Timeliness Database Table"
(collect_journal_stats ${system_type} ${title_file} /dev/null >> "${log}" 2>&1 && \
EndPhase || Abort) &
wait

echo -e "\n\nPipeline done after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes." | tee --append "${log}"
echo "*** COLLECT JOURNAL STATS PIPELINE DONE - $(date) ***" | tee --append "${log}"
