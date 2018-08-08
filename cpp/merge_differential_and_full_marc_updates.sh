#!/bin/bash
set -o errexit -o nounset


no_problems_found=1
function SendEmail {
    if [[ $no_problems_found -eq 0 ]]; then
        send_email --recipients="$email_address" --subject="$0 passed" --message-body="No problems were encountered."
        exit 0
    else
        send_email --recipients="$email_address" --subject="$0 failed" --message-body="Check the log file for details."
        exit 1
    fi
}
trap SendEmail EXIT


function Usage() {
    echo "Usage: $0 [--keep-intermediate-files] email_address"
    exit 1
}


# Argument processing
keep_itermediate_files=1
if [[ $# == 2 ]]; then
    if [[ $1 == "--keep-intermediate-files" ]]; then
        keep_itermediate_files=0
        email_address=$2
    else
        Usage
    fi
elif [[ $# != 1 ]]; then
    Usage
else
    email_address=$1
fi


function KeepIntermediateFiles() {
    if [[ $keep_itermediate_files = 0 ]]; then
        echo "--keep-intermediate-files"
    else
        echo ""
    fi
}


target_filename=Complete-MARC-$(date +%y%m%d).tar.gz
if [[ -e $target_filename ]]; then
    echo "Nothing to do: ${target_filename} already exists."
    exit 0
fi
echo "Creating ${target_filename}"


input_filename=$(generate_merge_order | head --lines=1)
declare -i counter=0
for update in $(generate_merge_order | tail --lines=+2); do
    ((++counter))
    temp_filename=temp_filename.$BASHPID.$counter.tar.gz
    if [[ ${update:0:6} == "LOEPPN" ]]; then
        echo "Processing deletion list: $update"
        archive_delete_ids $(KeepIntermediateFiles) $input_filename $update $temp_filename
    else
        echo "Processing differential dump: $update"
        apply_differential_update $(KeepIntermediateFiles) $input_filename $update $temp_filename
    fi
    input_filename=$temp_filename
done


# If we did not execute the for-loop at all, $temp_filename is unset and we need to set it to the empty string:
temp_filename=${temp_filename:-}

if [ -z ${temp_filename} ]; then
    ln --symbolic --force $input_filename Complete-MARC-current.tar.gz
else
    mv $temp_filename $target_filename

    if [[ ! keep_itermediate_filenames ]]; then
        rm temp_filename.$BASHPID.*.tar.gz TA-*.tar.gz WA-*.tar.gz SA-*.tar.gz
    fi


    # Create symlink to newest complete dump:
    ln --symbolic --force $target_filename Complete-MARC-current.tar.gz
fi


no_problems_found=0
