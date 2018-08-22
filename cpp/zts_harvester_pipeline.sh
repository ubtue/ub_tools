#!/bin/bash
# Runs through the phases of the Zotero Harvester delivery pipeline.
set -o errexit -o nounset


function ExitHandler {
    echo "*** ZTS_HARVESTER DELIVERY PIPELINE FAILED ***" | tee --append "${log}"
    exit 1
}
trap ExitHandler SIGINT SIGTERM


function Usage {
    echo "usage: $0 flavour mode"
    echo "flavour = IxTheo|KrimDok"
    echo "mode = TEST|LIVE"
    exit 1
}


if [ $# != 2 ]; then
    Usage
fi


if [ "$1" = "IxTheo" ]; then
    harvester_group=IxTheo
elif [ "$1" = "KrimDok" ]; then
    harvester_group=KrimDok
else
    echo "unknown flavour '$1'"
    Usage
fi


if [ "$2" = "TEST" ]; then
    upload_directory=/pub/UBTuebingen_Import_Test/${harvester_group,,}_Test/
elif [ "$2" = "LIVE" ]; then
    upload_directory=/pub/UBTuebingen_Import/${harvester_group,,}/
else
    echo "unknown mode '$2'"
    Usage
fi


delivery_mode=$2
working_directory=/tmp
harvester_output_filename=zts_harvester-$(date +%y%m%d).xml
harvester_config_file=/usr/local/ub_tools/cpp/data/zts_harvester.conf
missing_metadata_tracker_output_filename=zts_harvester-$(date +%y%m%d)-missing-metadata.log


function StartPhase {
    if [ -z ${PHASE+x} ]; then
        PHASE=1
    else
        ((++PHASE))
    fi
    START=$(date +%s.%N)
    echo "*** Phase $PHASE: $1 - $(date) ***" | tee --append "${log}"
}


# Call with "CalculateTimeDifference $start $end".
# $start and $end have to be in seconds.
# Returns the difference in fractional minutes as a string.
function CalculateTimeDifference {
    start=$1
    end=$2
    echo "scale=2;($end - $start)/60" | bc --mathlib
}


function EndPhase {
    PHASE_DURATION=$(CalculateTimeDifference $START $(date +%s.%N))
    echo -e "Done after ${PHASE_DURATION} minutes.\n" | tee --append "${log}"
}


# Set up the log file:
logdir=/usr/local/var/log/tuefind
log="${logdir}/zts_harvester_pipeline.log"
rm -f "${log}"


OVERALL_START=$(date +%s.%N)


StartPhase "Harvest URLs"
(export LOGGER_FORMAT=no_decorations,strip_call_site; \
zts_harvester --min-log-level=INFO --delivery-mode=$delivery_mode --groups=$harvester_group \
              --output-file=$working_directory/$harvester_output_filename \
              $harvester_config_file >> "${log}" 2>&1)
EndPhase


StartPhase "Validate Generated Records"
find_missing_metadata $working_directory/$harvester_output_filename \
                      $working_directory/$missing_metadata_tracker_output_filename >> "${log}" 2>&1
EndPhase


StartPhase "Upload to BSZ Server"
upload_to_bsz_ftp_server.sh $working_directory/$harvester_output_filename \
                            $upload_directory >> "${log}" 2>&1
EndPhase


echo -e "\n\nPipeline done after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes." | tee --append "${log}"
echo "*** ZTS_HARVESTER DELIVERY PIPELINE DONE ***" | tee --append "${log}"
