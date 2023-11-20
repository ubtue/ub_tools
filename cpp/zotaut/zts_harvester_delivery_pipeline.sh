#!/bin/bash
# Runs through the phases of the Zotero Harvester delivery pipeline.
set -o errexit -o nounset -o pipefail


no_problems_found=1
function SendEmail {
    if [[ $no_problems_found -eq 0 ]]; then
        send_email --priority=low --recipients="$EMAIL_ADDRESS" \
                   --subject="$0 passed on $(hostname)" --message-body="No problems were encountered."
        exit 0
    else
        send_email --priority=high --recipients="$EMAIL_ADDRESS" \
                   --subject="$0 failed on $(hostname)" \
                   --message-body="Check the log file at /usr/local/var/log/tuefind/zts_harvester_delivery_pipeline.log for details."
        echo "*** ZTS_HARVESTER DELIVERY PIPELINE FAILED ***" | tee --append "$LOG"
        exit 1
    fi
}
trap SendEmail EXIT


function Usage {
    echo "usage: $0 mode email"
    echo "       mode = TEST|LIVE"
    echo "       email = email address to which notifications are sent upon (un)successful completion of the delivery pipeline"
    exit 1
}


if [ $# != 2 ]; then
    Usage
fi


if [ "$1" != "TEST" ] && [ "$1" != "LIVE" ]; then
    echo "unknown mode '$1'"
    Usage
fi


readonly DELIVERY_MODE="$1"
readonly EMAIL_ADDRESS="$2"
readonly WORKING_DIRECTORY="/tmp/zts_harvester_delivery_pipeline"

readonly HARVESTER_OUTPUT_DIRECTORY="$WORKING_DIRECTORY"
readonly HARVESTER_OUTPUT_FILENAME="zts_harvester-$(date +%y%m%d).xml"
readonly HARVESTER_CONFIG_FILE="/usr/local/var/lib/tuelib/zotero-enhancement-maps/zotero_harvester.conf"

readonly DEST_DIR_LOCAL_TEST="/mnt/ZE020110/FID-Projekte/Default_Test/"
readonly DEST_DIR_LOCAL_LIVE="/mnt/ZE020110/FID-Projekte/Default/"
readonly DEST_DIR_REMOTE_TEST="/2001/Default_Test/input/"
readonly DEST_DIR_REMOTE_LIVE="/2001/Default/input/"



function StartPhase {
    if [ -z ${PHASE+x} ]; then
        PHASE=1
    else
        ((++PHASE))
    fi
    START=$(date +%s.%N)
    echo "*** Phase $PHASE: $1 - $(date) ***" | tee --append "$LOG"
}


# Call with "CalculateTimeDifference $start $end".
# $start and $end have to be in seconds.
# Returns the difference in fractional minutes as a string.
function CalculateTimeDifference {
    start="$1"
    end="$2"
    echo "scale=2;($end - $start)/60" | bc --mathlib
}


function EndPhase {
    PHASE_DURATION=$(CalculateTimeDifference $START $(date +%s.%N))
    echo -e "Done after $PHASE_DURATION minutes.\n" | tee --append "$LOG"
}


function EndPipeline {
    echo -e "\n\nPipeline done after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes." | tee --append "$LOG"
    echo "*** ZTS_HARVESTER DELIVERY PIPELINE DONE ***" | tee --append "$LOG"
    no_problems_found=0
    exit 0
}


# Set up the log file:
LOGDIR=/usr/local/var/log/tuefind
LOG_FILENAME=$(basename "$0")
LOG="$LOGDIR/${LOG_FILENAME%.*}.log"
rm --force "$LOG"

# Cleanup files/folders from a previous run
mkdir --parents "$HARVESTER_OUTPUT_DIRECTORY"
rm --recursive --force --dir "$HARVESTER_OUTPUT_DIRECTORY/ixtheo"
rm --recursive --force --dir "$HARVESTER_OUTPUT_DIRECTORY/krimdok"
rm --recursive --force --dir "$HARVESTER_OUTPUT_DIRECTORY/ubtuebingen"


OVERALL_START=$(date +%s.%N)
declare -a source_filepaths
declare -a dest_filepaths
declare -a dest_filepaths_local

StartPhase "Harvest URLs"
LOGGER_FORMAT=no_decorations,strip_call_site \
BACKTRACE=1 \
UTIL_LOG_DEBUG=true \
zotero_harvester --min-log-level=DEBUG \
                 --output-directory="$HARVESTER_OUTPUT_DIRECTORY" \
                 --output-filename="$HARVESTER_OUTPUT_FILENAME" \
                 "$HARVESTER_CONFIG_FILE" \
                 "UPLOAD" \
                 "$DELIVERY_MODE" >> "$LOG" 2>&1
EndPhase


StartPhase "Validate Generated Records"
# Make sure journals with selective evaluation get the appropriate exception rules for validation
adjust_selective_evaluation_validation_rules ${HARVESTER_CONFIG_FILE} >> "$LOG" 2>&1
cd "$HARVESTER_OUTPUT_DIRECTORY"
counter=0
shopt -s nullglob
for d in */ ; do
    d="${d%/}"
    if [[ "$d" -ef "$HARVESTER_OUTPUT_DIRECTORY" ]]; then
        continue
    fi

    current_source_filepath="$HARVESTER_OUTPUT_DIRECTORY/$d/$HARVESTER_OUTPUT_FILENAME"

    # Output filenames MUST start with 'ixtheo_' or 'krimdok_', else BSZ will ignore it.
    # Also, since we deliver files only once a day, each file will be marked as "001".
    valid_records_output_filepath="$HARVESTER_OUTPUT_DIRECTORY/$d/${d}_zotero_$(date +%y%m%d)_001.xml"
    online_first_records_output_filepath="$HARVESTER_OUTPUT_DIRECTORY/$d/${d}_zotero_$(date +%y%m%d)_001_online_first.xml"
    invalid_records_output_filepath="$HARVESTER_OUTPUT_DIRECTORY/$d/${d}_zotero_$(date +%y%m%d)_001_errors.xml"
    invalid_records_log_filepath="${invalid_records_output_filepath}.log"
    LOGGER_FORMAT=no_decorations,strip_call_site \
    BACKTRACE=1 \
    UTIL_LOG_DEBUG=true \
    validate_harvested_records "--update-db-errors" "$current_source_filepath" "$valid_records_output_filepath" \
                               "$online_first_records_output_filepath" \
                               "$invalid_records_output_filepath" "$EMAIL_ADDRESS" 2>&1 | tee --append "$LOG" "$invalid_records_log_filepath"

    invalid_record_count=$(marc_size "$invalid_records_output_filepath" 2>> "$LOG")
    if [ "$invalid_record_count" != "0" ]; then
        if [ "$DELIVERY_MODE" = "TEST" ]; then
            cp "$invalid_records_output_filepath" "$invalid_records_log_filepath" "$DEST_DIR_LOCAL_TEST" >> "$LOG" 2>&1
        elif [ "$DELIVERY_MODE" = "LIVE" ]; then
            cp "$invalid_records_output_filepath" "$invalid_records_log_filepath" "$DEST_DIR_LOCAL_LIVE" >> "$LOG" 2>&1
        fi
    fi

    online_first_record_count=$(marc_size "$online_first_records_output_filepath" 2>> "$LOG")
    if [ "$online_first_record_count" != "0" ]; then
        if [ "$DELIVERY_MODE" = "TEST" ]; then
            cp "$online_first_records_output_filepath" "$DEST_DIR_LOCAL_TEST" >> "$LOG" 2>&1
        elif [ "$DELIVERY_MODE" = "LIVE" ]; then
            cp "$online_first_records_output_filepath" "$DEST_DIR_LOCAL_LIVE" >> "$LOG" 2>&1
        fi
    fi

    valid_record_count=$(marc_size "$valid_records_output_filepath" 2>> "$LOG")
    if [ "$valid_record_count" = "0" ]; then
        continue    # skip files with zero records
    fi

    source_filepaths[$counter]="$valid_records_output_filepath"
    if [ "$DELIVERY_MODE" = "TEST" ]; then
        dest_filepaths[$counter]="$DEST_DIR_REMOTE_TEST"
        dest_filepaths_local[$counter]="$DEST_DIR_LOCAL_TEST"
    elif [ "$DELIVERY_MODE" = "LIVE" ]; then
        dest_filepaths[$counter]="$DEST_DIR_REMOTE_LIVE"
        dest_filepaths_local[$counter]="$DEST_DIR_LOCAL_LIVE"
    fi
    counter=$((counter+1))
done

if [ "$counter" = "0" ]; then
    echo "No new records were harvested"
    EndPipeline
fi
EndPhase


StartPhase "Upload to BSZ Server"
counter=0
file_count=${#source_filepaths[@]}

while [ "$counter" -lt "$file_count" ]; do
    upload_to_bsz_ftp_server.py "${source_filepaths[counter]}" \
                                "${dest_filepaths[counter]}" >> "$LOG" 2>&1
    if [[ -d "${dest_filepaths_local[$counter]}" ]]; then
        cp "${source_filepaths[counter]}" "${dest_filepaths_local[$counter]}" >> "$LOG" 2>&1
    fi
    counter=$((counter+1))
done
EndPhase


StartPhase "Archive Sent Records"
for source_filepath in "${source_filepaths[@]}"; do
    LOGGER_FORMAT=no_decorations,strip_call_site \
    BACKTRACE=1 \
    UTIL_LOG_DEBUG=true \
    archive_sent_records "$source_filepath" >> "$LOG" 2>&1
done
EndPhase


StartPhase "Check for Overdue Articles"
LOGGER_FORMAT=no_decorations,strip_call_site \
BACKTRACE=1 \
UTIL_LOG_DEBUG=true \
journal_timeliness_checker "$HARVESTER_CONFIG_FILE" "no-reply@ub.uni-tuebingen.de" "$EMAIL_ADDRESS" >> "$LOG" 2>&1
EndPhase


EndPipeline
