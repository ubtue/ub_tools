#!/bin/bash
set -o errexit -o nounset


no_problems_found=1
log_file="/usr/local/var/log/tuefind/merge_differential_and_full_marc_updates.log"
function SendEmail {
    if [[ $no_problems_found -eq 0 ]]; then
        send_email --priority=very_low --recipients="$email_address" --subject="$0 passed on $(hostname)" --message-body="No problems were encountered."
        exit 0
    else
        send_email --priority=high --recipients="$email_address" --subject="$0 failed on $(hostname)"  \
                   --message-body="$(printf '%q' "$(echo -e "Check /usr/local/var/log/tuefind/merge_differential_and_full_marc_updates.log for details.\n\n" \
                                     "$(tail -20 ${log_file})" \
                                        "'\n'")")"
        exit 1
    fi
}
trap SendEmail EXIT


function Usage() {
    echo "Usage: $0 [--keep-intermediate-files] email_address"
    exit 1
}


function CleanUpStaleDirectories() {
    for stale_difference_archive_directory in $(exec 2>/dev/null; ls -1 *.tar.gz | sed -e 's/\.tar\.gz$//'); do
       if [[ -d $stale_difference_archive_directory ]]; then
           rm --recursive ${stale_difference_archive_directory}
       fi
    done

    for stale_stage_directory in $(exec 2>/dev/null; ls -1 --directory temp_directory*); do
          rm --recursive ${stale_stage_directory}
    done
}


function GetDateFromFilename() {
   filename="$1"
   echo $(ls -t ${filename} 2>/dev/null | head --lines=1 | sed --regexp-extended --expression='s/[^0-9]//g')
}


# Argument processing
KEEP_INTERMEDIATE_FILES=""
if [[ $# > 1 ]]; then
    if [[ $1 == "--keep-intermediate-files" ]]; then
        KEEP_INTERMEDIATE_FILES="--keep-intermediate-files"
    else
        Usage
    fi
    shift
fi
if [[ $# != 1 ]]; then
    Usage
fi
email_address=$1

# Must be the same path as in fetch_marc_updates.py and, if applicable, trigger_pipeline_script.sh
MUTEX_FILE=/usr/local/var/tmp/bsz_download_happened
if [ ! -e $MUTEX_FILE ]; then
    no_problems_found=0
    send_email --recipients="$email_address" --subject="Mutex file not found on $(hostname)" \
               --message-body="$MUTEX_FILE"' is missing => new data was probably not downloaded!'
    exit 0
fi

> ${log_file}

# Make sure we do not gobble up full dumps and pseudo full dumps
newest_full_dump_date=$(GetDateFromFilename "SA-MARC-*.tar.gz")
newest_pseudo_dump_date=$(GetDateFromFilename "Complete-MARC-*.tar.gz")
if [[ ${newest_full_dump_date} = ${newest_pseudo_dump_date} ]]; then
    echo "Newest full dump and newest pseudo dump may not have the same date"
    exit 1
fi

target_filename=Complete-MARC-$(date +%y%m%d).tar.gz
if [[ -e $target_filename ]]; then
    echo "Nothing to do: ${target_filename} already exists."
    rm --force $MUTEX_FILE
    exit 0
fi


echo "Clean up Stale Directories"
CleanUpStaleDirectories


echo "Creating ${target_filename}"

input_filename=$(generate_merge_order | head --lines=1)
#If applicable make sure both full dumps exist
if [[ "${input_filename:0:8}" = "SA-MARC-" && $(ls ${input_filename:0:8}*-${newest_full_dump_date}.tar.gz | wc --lines) != 2 ]]; then
   echo 'Incorrect number of SA-MARC-* candidates'
   exit 1
fi

# Create a subdirectory from the input archive:
extraction_directory="${input_filename%.tar.gz}"
mkdir "$extraction_directory"
cd "$extraction_directory"
tar xzf ../"$input_filename"
cd -

input_directory=$extraction_directory

declare -i counter=0
last_temp_directory=
temp_directory=
> entire_record_deletion.log
for update in $(generate_merge_order | tail --lines=+2); do
    ((++counter))
    temp_directory=temp_directory.$BASHPID.$counter
    if [[ ${update:0:6} == "LOEKXP" ]]; then
        echo "[$(date +%y%m%d-%R:%S)] Processing deletion list: $update"
        echo archive_delete_ids $KEEP_INTERMEDIATE_FILES $input_directory $update $temp_directory  entire_record_deletion.log
        archive_delete_ids $KEEP_INTERMEDIATE_FILES $input_directory $update $temp_directory entire_record_deletion.log
    else
        echo "[$(date +%y%m%d-%R:%S)] Processing differential dump: $update"
        echo apply_differential_update $KEEP_INTERMEDIATE_FILES $input_directory $update $temp_directory
        apply_differential_update $KEEP_INTERMEDIATE_FILES $input_directory $update $temp_directory
    fi
    if [[ -n "$last_temp_directory" ]]; then
        rm --recursive ${last_temp_directory}
    fi
    input_directory=$temp_directory
    last_temp_directory=$temp_directory
done


# If we did not execute the for-loop at all, $temp_directory is unset and we need to set it to the empty string:
temp_directory=${temp_directory:-}

if [ -z ${temp_directory} ]; then
    ln --symbolic --force $input_filename Complete-MARC-current.tar.gz
else
    rm --recursive "$extraction_directory"
    cd ${temp_directory}
    tar czf ../$target_filename *mrc
    cd ..
    rm --recursive ${temp_directory}

    if [ -z "{$KEEP_INTERMEDIATE_FILES}" ]; then
        rm temp_directory.$BASHPID.* TA-*.tar.gz WA-*.tar.gz SA-*.tar.gz
    fi

    # Create symlink to newest complete dump:
    ln --symbolic --force $target_filename Complete-MARC-current.tar.gz
fi


rm --force $MUTEX_FILE
no_problems_found=0
