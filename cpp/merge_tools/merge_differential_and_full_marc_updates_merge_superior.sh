#!/bin/bash
set -o errexit -o nounset


no_problems_found=1
function SendEmail {
    if [[ $no_problems_found -eq 0 ]]; then
        send_email --priority=very_low --recipients="$email_address" --subject="$0 passed on $(hostname)" --message-body="No problems were encountered."
        exit 0
    else
        send_email --priority=high --recipients="$email_address" --subject="$0 failed on $(hostname)" \
                   --message-body="Check /usr/local/var/log/tuefind/merge_differential_and_full_marc_updates.log for details."
        exit 1
    fi
}
trap SendEmail EXIT


function Usage() {
    echo "Usage: $0 [--keep-intermediate-files] [--skip-db-updates] email_address"
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


# Merges records in a bare, i.e. non-archive file
function MergePrintAndOnlineTitles() {
    local input_filename=$1
    local output_filename=$2
    local working_dir=$3
    merge_print_and_online ${SKIP_DB_UPDATES} ${input_filename} ${output_filename} ${working_dir}/missing_ppn_partners.list
}


# Replace the tit.mrc in an archive with a version with merged superior works"
function CreateArchiveWithMergedTitles() {
    local input_filename=$1
    local output_filename=$2
    local extraction_directory=$3
    echo "Merging Print and Online from archive \"${input_filename}\" and writing to archive \"${output_filename}\""
    working_dir=$(pwd)
    cd "$extraction_directory"
    tar xzf ../"$input_filename"
    MergePrintAndOnlineTitles tit.mrc tit_merged.mrc ${working_dir}
    mv tit_merged.mrc tit.mrc
    tar czf ../"$output_filename" *mrc
    cd -
}


# Argument processing
KEEP_INTERMEDIATE_FILES=
SKIP_DB_UPDATES=
if [[ $# > 1 ]]; then
    if [[ $1 == "--keep-intermediate-files" ]]; then
        KEEP_INTERMEDIATE_FILES="--keep-intermediate-files"
        shift
    fi
    if [[ $1 == "--skip-db-updates" ]]; then
        SKIP_DB_UPDATES="--skip-db-updates"
        shift
    fi
fi
if [[ $# != 1 ]]; then
    Usage
fi
email_address=$1


MUTEX_FILE=/usr/local/var/tmp/bsz_download_happened # Must be the same path as in fetch_marc_updates.py!
if [ ! -e $MUTEX_FILE ]; then
    trap - EXIT # Don't send another email due to the trap.
    send_email --recipients="$email_address" --subject="Mutex file not found on $(hostname)" \
               --message-body="$MUTEX_FILE is missing => new data was probably not downloaded!"
    exit 0
fi


target_filename=Complete-MARC-merged-$(date +%y%m%d).tar.gz
if [[ -e $target_filename ]]; then
    echo "Nothing to do: ${target_filename} already exists."
    rm --force $MUTEX_FILE
    exit 0
fi

echo "Clean up Stale Directories"
CleanUpStaleDirectories


echo "Creating ${target_filename}"


input_filename=$(generate_merge_order | head --lines=1)

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
for update in $(generate_merge_order | tail --lines=+2); do
    ((++counter))
    temp_directory=temp_directory.$BASHPID.$counter
    if [[ ${update:0:6} == "LOEKXP" ]]; then
        echo "[$(date +%y%m%d-%R:%S)] Processing deletion list: $update"
        echo archive_delete_ids $KEEP_INTERMEDIATE_FILES $input_directory $update $temp_directory
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


target_filename_unmerged=${target_filename/-merged/}


if [ -z ${temp_directory} ]; then # We did not execute the for-loop above!
    echo 'for-loop has not been executed!'
    if [[ ! -f $target_filename ]]; then
        cp $input_filename $target_filename_unmerged
        CreateArchiveWithMergedTitles $input_filename $target_filename $extraction_directory
    fi
    ln --symbolic --force $target_filename Complete-MARC-current.tar.gz
else
    echo 'for-loop has been executed!'
    rm --recursive "$extraction_directory"
    cd ${temp_directory}
    tar czf ../$target_filename_unmerged *mrc
    MergePrintAndOnlineTitles tit.mrc tit_merged.mrc .
    mv tit_merged.mrc tit.mrc
    tar czf ../$target_filename *mrc
    cd ..

    if [[ ! keep_itermediate_filenames ]]; then
        echo 'cleaning up all working files'
        rm --recursive temp_directory.$BASHPID.* TA-*.tar.gz WA-*.tar.gz SA-*.tar.gz
    else
        echo 'removing last working directory only'
        rm --recursive ${temp_directory}
    fi

    # Create symlink to newest complete dump:
    ln --symbolic --force $target_filename Complete-MARC-current.tar.gz
fi


rm --force $MUTEX_FILE
no_problems_found=0
