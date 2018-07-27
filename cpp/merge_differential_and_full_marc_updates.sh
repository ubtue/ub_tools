#!/bin/bash
set -o errexit -o nounset


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


declare -i counter=0
function GetNextTempFilename() {
    ((++counter))
    echo temp_file.$BASHPID.$counter.tar.gz
}


if [[ -z $(printenv TUEFIND_FLAVOUR) ]]; then
    echo "You need to set the environment variable TUEFIND_FLAVOUR in order to run this script!"
    exit 1
fi


target_filename=Complete-MARC-${TUEFIND_FLAVOUR}-$(date +%y%m%d).tar.gz
if [[ -e $target_filename ]]; then
    echo "Nothing to do: ${target_filename} already exists."
    exit 0
fi
echo "Creating ${target_filename}"


input_file=$(generate_merge_order | head --lines=1)
for update in $(generate_merge_order | tail --lines=+2); do
    output_file=$(GetNextTempFilename)
    if [[ ${update:0:6} == "LOEPPN" ]]; then
        echo "Processing deletion list: $update"
        archive_delete_ids $(KeepIntermediateFiles) $input_file $update $output_file
    else
        echo "Processing differential dump: $update"
        apply_differential_update $(KeepIntermediateFiles) $input_file $update $output_file
    fi
    input_file=$output_file
done


mv $output_file $target_filename


if [[ ! keep_itermediate_files ]]; then
    rm temp_file.$BASHPID.*.tar.gz TA-*.tar.gz WA-*.tar.gz SA-*.tar.gz
fi


# Create symlink to newest complete dump:
rm --force Complete-MARC-ixtheo-current.tar.gz
ln --symbolic $target_filename Complete-MARC-ixtheo-current.tar.gz

no_problems_found=0


function SendEmail {
    if [[ -n $no_problems_found ]]; then
        exit 0
    else
        send_email --recipients="$email_address" --subject="$0 failed" --message-body="Check the log file for details."
    fi 
}
trap SendEmail EXIT
