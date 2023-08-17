#/bin/bash
set -o errexit -o nounset -o pipefail



if [ $# != 2 ]; then
    echo "usage: $0 brill_reference_works_dir output_dir"
    exit 1
fi


function RemoveTempFiles {
   for tmpfile in ${tmpfiles[@]}; do
       rm ${tmpfile}
   done
}


trap RemoveTempFiles EXIT

tmpfiles=()

archive_dir="$1"
output_dir="$2"
conversion_script="./convert_brill_refwork_metadata_to_marc.sh"
clean_script="./clean_and_augment_brill_marc_records.sh"
augment_authors="./add_author_associations"
get_author_associations="./associate_authors.sh"
associate_rgg4_titles_script="./associate_rgg4_titles.sh"
replace_title_by_id_program="./replace_title_by_id"
rgg4_multicandidates_rewrite_file="rgg4_daten/rgg4_multicandidates_rewrite_checked_clean.txt"
rgg4_unassociated_rewrite_file="rgg4_daten/rgg4_unassociated_rewrite_erl_fixes.txt"
rgg4_multicandidates_manual_rewrite_file="rgg4_daten/rgg4_multicandidates_manual_rewrite_stripped_erl_clean.txt"
rgg4_id_title_replacements="rgg4_daten/rgg4_id_title_replacements.txt"
rgg4_id_title_replacements_authors="rgg4_daten/rgg4_id_title_replacements_authors.txt"
rgg4_id_title_replacements_authors_comma_end="rgg4_daten/rgg4_id_title_replacements_authors_comma_end.txt"


REFWORKS=$(printf "%s" '.*\(BDRO\|BEJO\|BEHO\|BERO\|BESO\|ECO\|EECO\|' \
                       'EGPO\|EJIO\|ELRO\|ENBO\|LKRO\|RGG4\|RPPO\|VSRO\|WCEO' \
                       '\)\.\(zip\|ZIP\)$')


echo "All REFWORKS: " ${REFWORKS}
for archive_file in $(find ${archive_dir} -regex $(printf "%s" ${REFWORKS}) -print); do
    archive_name=$(basename ${archive_file%.*})
    echo "ARCHIVE_NAME: " ${archive_name}
    archive_tmp_file1=$(mktemp -t marc_convert_${archive_name}_XXXXX.xml)
    tmpfiles+=(${archive_tmp_file1})
    echo ${conversion_script} ${archive_file} ${archive_tmp_file1}
    ${conversion_script} ${archive_file} ${archive_tmp_file1}
        archive_tmp_file2=$(echo ${archive_tmp_file1} |
                        sed -r -e 's/marc_convert_/marc_convert_clean_/')
    echo ${clean_script} ${archive_tmp_file1} ${archive_tmp_file2}
    ${clean_script} ${archive_tmp_file1} ${archive_tmp_file2}
    tmpfiles+=(${archive_tmp_file2})
    author_associations="./daten/${archive_name}_associated_authors.txt"
    if [ ! -s ${author_associations} ]; then
       mkdir -p $(dirname ${author_associations})
       echo ${get_author_associations} ${archive_tmp_file2} ${author_associations}
       ${get_author_associations} ${archive_tmp_file2} ${author_associations}
    fi
    outfile=${output_dir}/ixtheo_brill_${archive_name}_$(date +'%y%m%d')_001.xml
    echo ${augment_authors} ${archive_tmp_file2} ${outfile} ${author_associations}
    ${augment_authors} ${archive_tmp_file2} ${outfile} ${author_associations}
    # Special title rewriting for RGG4
    if [ ${archive_name} = "RGG4" ]; then
        echo "Fixing RGG4"
        rgg4_rewrite_file=$(echo ${archive_tmp_file2} | \
                    sed -r -e 's/marc_convert_/rgg4_rewrite_file_/' |\
                    sed -r -e 's/\.xml$/.txt/')
        tmpfiles+=(${rgg4_rewrite_file})
        ${associate_rgg4_titles_script} ./rgg4_daten/orig_titles_230223.txt ./rgg4_daten/web_entries_230223.txt ${rgg4_rewrite_file}
        # Normalize strange spaces
        cat ${outfile} | \
            sed "s/[$(echo -ne '\u2000\u2001\u2002\u2002\u2003\u2004\u2005\u2006\u2007|\u2008\u2009\u200A\u200B')]/ /g" | \
            sponge ${outfile}
        tmp_stdout="/tmp/stdout.xml"
        ln --symbolic /dev/stdout ${tmp_stdout}
        tmpfiles+=(${tmp_stdout})
        marc_filter ${outfile} ${tmp_stdout} --replace 245a \
                     <(cat ${rgg4_rewrite_file} ${rgg4_multicandidates_rewrite_file} \
                       ${rgg4_unassociated_rewrite_file} ${rgg4_multicandidates_manual_rewrite_file} | sed 's/^/\^/') \
                     | sponge ${outfile}
        ${replace_title_by_id_program} ${outfile} ${tmp_stdout} \
             <(cat ${rgg4_id_title_replacements} ${rgg4_id_title_replacements_authors} ${rgg4_id_title_replacements_authors_comma_end}) \
             | sponge ${outfile}
        marc_augmentor ${outfile} ${tmp_stdout} --insert-field '912a:NOMM' | sponge ${outfile}
        # Remove annoying dots at end (negative lookbehind to skip the expressin "sen."
        marc_filter ${outfile} ${tmp_stdout} --globally-substitute '245a' '((?<!se)[a-z])[.]$' '\1' \
             | sponge ${outfile}
    fi
    #Generate more easily readable text representation
    marc_format_outfile=${outfile%.xml}.txt
    marc_grep ${outfile} 'if "001"==".*" extract *' traditional > ${marc_format_outfile}
done
