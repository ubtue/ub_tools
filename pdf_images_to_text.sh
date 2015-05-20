#!/bin/bash

if [ $# != 3 ]; then
    echo "usage: $0 image_pdf_filename output_text_filename tesseract_language_codes"
    exit 1
fi

temp_dir_name=$(mktemp -d)
trap 'rm -rf "${temp_dir_name}"' EXIT

rm -f "$2"
pdfimages "$1" "${temp_dir_name}/out"
for image in "${temp_dir_name}/out"*; do
    tesseract "$image" stdout -l "$3" >> "$2"
done

exit 0
