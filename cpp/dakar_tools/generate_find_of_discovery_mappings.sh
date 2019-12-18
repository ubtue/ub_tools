#!/bin/bash
shopt -s extglob

FILES_IMPORT_1="PPN-DaKaR-ZS-Reihen_1_Import_table*.csv"
FILE_IMPORT_2="PPN-DaKaR-ZS-Reihen_2_Import_table1.csv"
TMPDIR="./all_tables"
CONVERT_TO_K10PLUS_PPN_FILE="PPN-DaKaR-ZS-Reihen_1_Import.csv" 


#Convert to UTF-8 CSV-File
for file in "$FILES_IMPORT_1"; do
    sed -e '1,2d' $file  | awk -F',' \
        '{"kchashmgr get /usr/local/tmp/KCKONKORDANZ.db " $1 | getline new_ppn; \
          printf "%s", new_ppn; for (column = 2; column <=NF; ++column) printf FS$column; print NL}'
done

# Remove ZDB and Zeder column to match our PPN -> Dakar-Abbreviations
# We use gawk since ordinary cut does not cope with comma in quotes
gawk '{print $1, $4}' FPAT="([^,]+)|(\"[^\"]+\")" OFS="," "$FILE_IMPORT_2" | sed -e '1,2d'

