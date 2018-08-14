#!/bin/bash
set -o errexit -o nounset


no_problems_found=1
function SendEmail {
    if [[ $no_problems_found -eq 0 ]]; then
        send_email --recipients="$email_address" --subject="$0 passed on $(hostname)" --message-body="No problems were encountered."
        exit 0
    else
        send_email --recipients="$email_address" --subject="$0 failed on $(hostname)" \
                   --message-body="Check /usr/local/var/log/tuefind/merge_differential_and_full_marc_updates.log for details."
        exit 1
    fi
}
trap SendEmail EXIT


function Usage() {
    echo "Usage: $0 [--keep-intermediate-files] [--use-subdirectories] email_address"
    exit 1
}


# Argument processing
KEEP_ITERMEDIATE_FILES=
USE_SUBDIRECTORIES=
if [[ $# > 1 ]]; then
    if [[ $1 == "--keep-intermediate-files" ]]; then
        KEEP_ITERMEDIATE_FILES="--keep-intermediate-files"
    elif [[ $1 == "--use-subdirectories" ]]; then
        USE_SUBDIRECTORIES="--use-subdirectories"
    else
        Usage
    fi
    shift
fi
if [[ $# > 1 ]]; then
    if [[ $1 == "--use-subdirectories" ]]; then
        USE_SUBDIRECTORIES="--use-subdirectories"
    else
        Usage
    fi
    shift
fi
if [[ $# != 1 ]]; then
    Usage
fi
email_address=$1


target_filename=Complete-MARC-$(date +%y%m%d).tar.gz
if [[ -e $target_filename ]]; then
    echo "Nothing to do: ${target_filename} already exists."
    exit 0
fi
echo "Creating ${target_filename}"


input_filename=$(generate_merge_order | head --lines=1)
if [[ -n ${USE_SUBDIRECTORIES} ]]; then
    extraction_directory=${input_filename%.tar.gz}
    cd $extraction_directory
    tar xzf ../$input_filename
    cd -
fi
declare -i counter=0
last_temp_filename=
for update in $(generate_merge_order | tail --lines=+2); do
    ((++counter))
    temp_filename=temp_filename.$BASHPID.$counter.tar.gz
    if [[ ${update:0:6} == "LOEPPN" ]]; then
        echo "Processing deletion list: $update"
        archive_delete_ids $KEEP_ITERMEDIATE_FILES $USE_SUBDIRECTORIES $input_filename $update $temp_filename
    else
        echo "Processing differential dump: $update"
        apply_differential_update $KEEP_ITERMEDIATE_FILES $USE_SUBDIRECTORIES $input_filename $update $temp_filename
    fi
    if [[ -n "$last_temp_filename" ]]; then
        if [[ -z ${USE_SUBDIRECTORIES} ]]; then
            rm "$last_temp_filename"
        else
            rm -r ${last_temp_filename%.tar.gz}
        fi
    fi
    input_filename=$temp_filename
    last_temp_filename=$temp_filename
done


# If we did not execute the for-loop at all, $temp_filename is unset and we need to set it to the empty string:
temp_filename=${temp_filename:-}

if [ -z ${temp_filename} ]; then
    ln --symbolic --force $input_filename Complete-MARC-current.tar.gz
else
    if [[ -z ${USE_SUBDIRECTORIES} ]]; then
        mv $temp_filename $target_filename
    else
        tar czf $target_filename ${temp_filename%.tar.gz}/*raw
        rm -r ${temp_filename%.tar.gz}
    fi

    if [[ ! keep_itermediate_filenames ]]; then
        rm temp_filename.$BASHPID.*.tar.gz TA-*.tar.gz WA-*.tar.gz SA-*.tar.gz
    fi


    # Create symlink to newest complete dump:
    ln --symbolic --force $target_filename Complete-MARC-current.tar.gz
fi


no_problems_found=0
