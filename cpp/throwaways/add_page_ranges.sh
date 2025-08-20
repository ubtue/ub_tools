#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage $0: marc.xml"
    exit 1
fi

PDFS_DIR="pdfs"
marc_file="$1"
output_file="ixtheo_zotero_$(date +%Y%m%d)_001.xml"


# Select lines 151 to 1000 (an even range) from the output, since each 856u entry produces two lines;
#marc_grep $1 'if "936h" is_missing extract "856u"' | sed -e  's/:856u:/\n/' | sed -n '151,1000p' | xargs -n 2 ./get_pdf.sh "$PDFS_DIR"
marc_grep $1 'if "936h" is_missing extract "856u"' | sed -e  's/:856u:/\n/' | xargs -n 2 ./get_pdf.sh "$PDFS_DIR"
ls -1 "$PDFS_DIR"/*.pdf | xargs -I'{}' python3 convert_pdf_to_md.py '{}'
eval marc_augmentor $1 $output_file $(ls -1 "$PDFS_DIR"/*.md | xargs -I'{}' ./extract_page_ranges.sh '{}' | awk $'{print "--add-subfield-if \'936h:" ($2=="" ? "XXXXX" : $2) "\' " "\'001:" $1 "\'"}'| tr '\n' ' ') 
