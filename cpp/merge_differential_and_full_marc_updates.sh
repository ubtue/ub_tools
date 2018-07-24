#!/bin/bash
set -o errexit -o nounset

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
for f in $(generate_merge_order | tail --lines=+2); do
    if [[ ${f:0:6} == "LOEPPN" ]]; then
        echo "Processing deletion list: $f"
    else
        echo "Processing differential dump: $f"
        echo "apply_differential_update $last_temp_file $f $output_file"
    fi
    last_temp_file=$f
done

echo mv $output_file $target_filename
