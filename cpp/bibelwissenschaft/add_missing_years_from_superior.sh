#!/bin/bash
# Add year from superior work for records where the original published_at-field in the database is missing

IN_FILE="daten/bibwiss12_with_author_cleaned.xml"
OUT_FILE="daten/bibwiss14_with_author_cleaned.xml"

candidate_ppns=$(marc_grep daten/ixtheo_bibwiss_221019_001.xml 'if "264c" is_missing extract "001"' traditional | awk '{print $2}')

bbi_ppns=$(printf '%s\n' "${candidate_ppns[@]}" | grep '^BBI')
bre_ppns=$(printf '%s\n' "${candidate_ppns[@]}" | grep '^BRE')

bbi_regex='001:'$(echo "${bbi_ppns}" | paste -sd "|")
bre_regex='001:'$(echo "${bre_ppns}" | paste -sd "|")
marc_augmentor "${IN_FILE}" "${OUT_FILE}" \
     --insert-field-if '264c:2005' "${bbi_regex}" \
     --add-subfield-if '773g:2005' "${bbi_regex}" \
     --insert-field-if '264c:2015' "${bre_regex}" \
     --add-subfield-if '773g:2015' "${bre_regex}"
