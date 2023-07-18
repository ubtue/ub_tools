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
rgg4_multicandidates_rewrite_file="rgg4_daten/rgg4_multicandidates_rewrite_checked_clean.txt"
rgg4_unassociated_rewrite_file="rgg4_daten/rgg4_unassociated_rewrite_erl.txt"
rgg4_multicandidates_manual_rewrite_file="/dev/null"


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
        tmp_stdout="/tmp/stdout.xml"
        ln --symbolic /dev/stdout ${tmp_stdout}
        tmpfiles+=(${tmp_stdout})
        marc_filter ${outfile} ${tmp_stdout} --replace 245a \
                     <(cat ${rgg4_rewrite_file} ${rgg4_multicandidates_rewrite_file} \
                       ${rgg4_unassociated_rewrite_file} ${rgg4_multicandidates_manual_rewrite_file}) \
                     | sponge ${outfile}
    fi
    #Generate more easily readable text representation
    marc_format_outfile=${outfile%.xml}.txt
    marc_grep ${outfile} 'if "001"==".*" extract *' traditional > ${marc_format_outfile}
done
