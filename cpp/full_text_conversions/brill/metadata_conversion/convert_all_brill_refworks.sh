#/bin/bash
set -o errexit -o nounset -o pipefail

trap RemoveTempFiles EXIT


if [ $# != 2 ]; then
    echo "usage: $0 brill_reference_works_dir output_dir"
    exit 1
fi


function RemoveTempFiles {
   for tmpfile in ${tmpfiles[@]}; do
       rm ${tmpfile}
   done
}

tmpfiles=()

archive_dir="$1"
output_dir="$2"
conversion_script="./convert_brill_refwork_metadata_to_marc.sh"
clean_script="./clean_and_augment_brill_marc_records.sh"
augment_authors="./add_author_associations"
get_author_associations="./associate_authors.sh"


for archive_file in $(find ${archive_dir} -regex '.*\(BDRO\|BEJO\|BEHO\|EECO\)\.\(zip\|ZIP\)$' -print); do
    archive_name=$(basename ${archive_file%.*})
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
done

