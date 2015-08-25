#!/bin/bash
set -o errexit -o nounset

if [ $# != 2 -a $# != 3 ]; then
    echo "usage: $0 image_pdf_filename output_text_filename [tesseract_language_code(s)]"
    exit 1
fi

temp_dir_name=$(mktemp -d)
trap 'rm -rf "${temp_dir_name}"' SIGTERM EXIT

rm -f "$2"
pdfimages "$1" "${temp_dir_name}/out"
for image in "${temp_dir_name}/out"*; do
    if [ $# == 2 ]; then
	tesseract "$image" stdout >> "$2"
    else
	tesseract "$image" stdout -l "$3" >> "$2"
    fi
done

exit 0
