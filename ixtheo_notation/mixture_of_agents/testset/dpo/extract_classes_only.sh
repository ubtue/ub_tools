#!/bin/bash

if [ $# != 2 ]; then
    echo "Usage $0 input_dir output_dir"
    exit 1
fi

input_dir="$1"
output_dir="$2"

ls -1 ${input_dir} | xargs -P 20 -I'{}' \
      /bin/bash -c '
      input_dir="$2"
      infile=$(realpath ${input_dir}/$1); \
      output_dir="$3"
      outfile=${output_dir}/$1; \
      source ../../bin/activate; \
      ./extract_plain_classes.sh ${infile} > ${outfile}' \
      _ '{}' ${input_dir} ${output_dir}

