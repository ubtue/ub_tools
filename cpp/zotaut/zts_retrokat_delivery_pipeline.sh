#!/bin/bash
# Runs through the phases of the Zotero Harvester delivery pipeline in the Retrokat context.
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
                   --message-body="Check the log file at /usr/local/var/log/tuefind/zts_retrokat_harvester_delivery_pipeline.log for details."
        echo "*** ZTS_RETROKAT_HARVESTER DELIVERY PIPELINE FAILED ***" | tee --append "$LOG"
        exit 1
    fi
}
trap SendEmail EXIT


function Usage {
    echo "usage: $0 journal email [--skip-sync-legacy] [--skip-bsz-upload]"
    echo "       journal = journal name to be harvested"
    echo "       email = email address to which notifications are sent upon (un)successful completion of the delivery pipeline"
    echo "       [--skip-sync-legacy] = optional flag to skip the sync_legacy_records step"
    echo "       [--skip-bsz-upload] = optional flag to skip the upload to BSZ FTP server step"
    exit 1
}


if [ $# -lt 2 ]; then
    Usage
fi


readonly JOURNAL_NAME="$1"
readonly EMAIL_ADDRESS="$2"

SKIP_SYNC_LEGACY=0
SKIP_BSZ_UPLOAD=0

for arg in "${@:3}"; do
    case "$arg" in
        --skip-sync-legacy) SKIP_SYNC_LEGACY=1 ;;
        --skip-bsz-upload)  SKIP_BSZ_UPLOAD=1 ;;
    esac
done

readonly WORKING_DIRECTORY="/tmp/zts_retrokat_harvester_delivery_pipeline"

if [[ "$JOURNAL_NAME" == *:* ]]; then
    SHORT_JOURNAL_NAME="${JOURNAL_NAME%%:*}"
else
    SHORT_JOURNAL_NAME="$JOURNAL_NAME"
fi
SHORT_JOURNAL_NAME=$(echo "$SHORT_JOURNAL_NAME" | sed -E '
    s/[[:space:]]+/_/g;
    s/[^A-Za-z0-9._-]//g;
    s/_+/_/g;
    s/_$//
')

readonly HARVESTER_OUTPUT_DIRECTORY="$WORKING_DIRECTORY"
readonly HARVESTER_OUTPUT_FILENAME="zts_retrokat_harvester-$(date +%y%m%d).xml"
readonly HARVESTER_CONFIG_FILE="/usr/local/var/lib/tuelib/zotero-enhancement-maps/zotero_harvester.conf"

readonly GIT_REPO_NAME="retrokat-daten"
readonly GIT_REPO_URL="/mnt/ZE020110/FID-Projekte/Retrokat-Daten/$GIT_REPO_NAME.git"
readonly LOCAL_REPO_PATH="$WORKING_DIRECTORY/$GIT_REPO_NAME"
readonly DEST_DIR_LOCAL_RETROKAT="$LOCAL_REPO_PATH/$SHORT_JOURNAL_NAME"
readonly DEST_DIR_REMOTE_RETROKAT="/2001/Default_Test/input/"



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
    echo "*** ZTS_RETROKAT_HARVESTER DELIVERY PIPELINE DONE ***" | tee --append "$LOG"
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

# Clone / Update git repository
if [ ! -d "$LOCAL_REPO_PATH/.git" ]; then
    echo "Cloning $GIT_REPO_NAME into $LOCAL_REPO_PATH..." | tee --append "$LOG"
    git clone "$GIT_REPO_URL" "$LOCAL_REPO_PATH" >> "$LOG" 2>&1
else
    echo "Updating $GIT_REPO_NAME repo in $LOCAL_REPO_PATH..." | tee --append "$LOG"
    git -C "$LOCAL_REPO_PATH" pull >> "$LOG" 2>&1
fi

mkdir -p "$DEST_DIR_LOCAL_RETROKAT"

OVERALL_START=$(date +%s.%N)
declare -a harvester_output
declare -a source_filepaths
declare -a dest_filepaths
declare -a dest_filepaths_local

if [ "$SKIP_SYNC_LEGACY" -eq 0 ]; then
    StartPhase "Sync Legacy Records"
    sync_legacy_records --harvester-conf "$HARVESTER_CONFIG_FILE" \
                        "$JOURNAL_NAME" >> "$LOG" 2>&1
    EndPhase
fi

StartPhase "Harvest URLs"
LOGGER_FORMAT=no_decorations,strip_call_site \
BACKTRACE=1 \
UTIL_LOG_DEBUG=true \
zotero_harvester --min-log-level=DEBUG \
                 --output-directory="$HARVESTER_OUTPUT_DIRECTORY" \
                 --output-filename="$HARVESTER_OUTPUT_FILENAME" \
                 "$HARVESTER_CONFIG_FILE" \
                 "JOURNAL" \
                 "$JOURNAL_NAME" >> "$LOG" 2>&1
EndPhase


StartPhase "Validate Generated Records"
# Make sure journals with selective evaluation get the appropriate exception rules for validation
adjust_selective_evaluation_validation_rules ${HARVESTER_CONFIG_FILE} >> "$LOG" 2>&1
cd "$HARVESTER_OUTPUT_DIRECTORY"
counter=0
shopt -s nullglob
for d in */ ; do
    d="${d%/}"

    if [[ "$d" == "$GIT_REPO_NAME" ]]; then
        echo "Skipping $d." | tee --append "$LOG"
        continue
    fi

    if [[ "$d" -ef "$HARVESTER_OUTPUT_DIRECTORY" ]]; then
        continue
    fi

    current_source_filepath="$HARVESTER_OUTPUT_DIRECTORY/$d/$HARVESTER_OUTPUT_FILENAME"

    # Output filenames MUST start with 'ixtheo_' or 'krimdok_', else BSZ will ignore it.
    timestamp=$(date +%y%m%d_%H%M%S_%N)
    valid_records_output_filepath="$HARVESTER_OUTPUT_DIRECTORY/$d/${d}_zotero_$(date +%y%m%d)_001.xml"
    online_first_records_output_filepath="$HARVESTER_OUTPUT_DIRECTORY/$d/${d}_zotero_$(date +%y%m%d)_001_online_first.xml"
    invalid_records_output_filepath="$HARVESTER_OUTPUT_DIRECTORY/$d/${d}_zotero_$(date +%y%m%d)_001_errors.xml"
    invalid_records_log_filepath="${invalid_records_output_filepath}.log"
    final_harvester_output_filepath="$HARVESTER_OUTPUT_DIRECTORY/$d/${d}_retrokat_$(timestamp)_001.xml"
    LOGGER_FORMAT=no_decorations,strip_call_site \
    BACKTRACE=1 \
    UTIL_LOG_DEBUG=true \
    validate_harvested_records "--update-db-errors" "$current_source_filepath" "$valid_records_output_filepath" \
                               "$online_first_records_output_filepath" \
                               "$invalid_records_output_filepath" "$EMAIL_ADDRESS" 2>&1 | tee --append "$LOG" "$invalid_records_log_filepath"

    invalid_record_count=$(marc_size "$invalid_records_output_filepath" 2>> "$LOG")
    if [ "$invalid_record_count" != "0" ]; then
        cp "$invalid_records_log_filepath" "$DEST_DIR_LOCAL_RETROKAT" >> "$LOG" 2>&1
    fi

    online_first_record_count=$(marc_size "$online_first_records_output_filepath" 2>> "$LOG")

    valid_record_count=$(marc_size "$valid_records_output_filepath" 2>> "$LOG")
    if [ "$valid_record_count" = "0" ]; then
        continue    # skip files with zero records
    fi
    cp "$valid_records_output_filepath" "$DEST_DIR_LOCAL_RETROKAT" >> "$LOG" 2>&1

    # Construct prefixed filename for BSZ Upload
    mv "$current_source_filepath" "$final_harvester_output_filepath" >> "$LOG" 2>&1

    harvester_output[$counter]="$final_harvester_output_filepath"
    source_filepaths[$counter]="$valid_records_output_filepath"
    dest_filepaths[$counter]="$DEST_DIR_REMOTE_RETROKAT"
    dest_filepaths_local[$counter]="$DEST_DIR_LOCAL_RETROKAT"
    counter=$((counter+1))
done

if [ "$counter" = "0" ]; then
    echo "No new records were harvested"
    EndPipeline
fi
EndPhase

StartPhase "Rename, Move and optional Upload to BSZ Server"
counter=0
file_count=${#harvester_output[@]}

while [ "$counter" -lt "$file_count" ]; do
    if [[ -d "${dest_filepaths_local[$counter]}" ]]; then
        cp "${harvester_output[counter]}" "${dest_filepaths_local[$counter]}" >> "$LOG" 2>&1
    fi
    if [ "$SKIP_BSZ_UPLOAD" -eq 0 ]; then
        upload_to_bsz_ftp_server.py "${harvester_output[counter]}" \
                                    "${dest_filepaths[counter]}" >> "$LOG" 2>&1
    fi
    counter=$((counter+1))
done
EndPhase


StartPhase "Commit and Push to Git"

cd "$LOCAL_REPO_PATH"
git add . >> "$LOG" 2>&1

if ! git diff --cached --quiet; then
    git commit -m "Update: Records for $JOURNAL_NAME on $(date +%Y-%m-%d)" >> "$LOG" 2>&1
    git push >> "$LOG" 2>&1
else
    echo "No changes to commit." | tee --append "$LOG"
fi

EndPhase


StartPhase "Archive Sent Records"
for source_filepath in "${source_filepaths[@]}"; do
    LOGGER_FORMAT=no_decorations,strip_call_site \
    BACKTRACE=1 \
    UTIL_LOG_DEBUG=true \
    archive_sent_records "$source_filepath" >> "$LOG" 2>&1
done
EndPhase


EndPipeline
