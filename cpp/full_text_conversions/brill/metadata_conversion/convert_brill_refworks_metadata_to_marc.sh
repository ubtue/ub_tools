#/bin/bash
set -o errexit -o nounset -o pipefail

if [ $# != 2 ]; then
    echo "usage: $0 brill_reference_work.zip output.xml"
    exit 1
fi

archive_file="$1"
output_file="$2"
PPN_BASE=$(basename ${archive_file} | sed -r -e 's/([[:alnum:]]+)\.zip/\1/i')
archive_contents=$(7z l ${archive_file} | grep "${PPN_BASE}/metadata" | grep 'xml$' | awk '{ print $6 }')
TOOL_BASE_PATH="/usr/local/ub_tools/cpp/full_text_conversions/brill/metadata_conversion"
DC_TO_MODS_XSLT_FILE="xsl/DC_MODS3-7_XSLT1-0_ubtue.xsl"
MODS_TO_MARC_XSLT_FILE="xsl/MODS3-7_MARC21slim_XSLT1-0_ubtue_brill.xsl"

> ${output_file}
echo '<?xml version="1.0" encoding="UTF-8"?>' >> ${output_file}
echo '<collection>' >> ${output_file}
i=0
for content in ${archive_contents}; do
     i=$((i+1))
     ppn=$(printf "${PPN_BASE}%06d" ${i})
     echo ${ppn}
     echo ${content}
     7z x -so ${archive_file} ${content} | \
         sed -r -e 's#<metadata#<oai_dc:dc#' | \
         sed -r -e 's#</metadata>#</oai_dc:dc>#' | \
         xmlstarlet tr ${TOOL_BASE_PATH}/${DC_TO_MODS_XSLT_FILE} | \
         xmlstarlet tr ${TOOL_BASE_PATH}/${MODS_TO_MARC_XSLT_FILE} | \
         xmlstarlet ed -O -a //marc:leader -t elem -n 'marc:controlfield' -v "${ppn}"  \
                    --var new_node '$prev' -i '$new_node' -t attr -n "tag" -v "001" | \
         sed -r -e 's/(<[/]?)marc:/\1/g' | \
         sed -r -e 's/(<record).*>/\1>/g' >> ${output_file} 
done
echo '</collection>' >> ${output_file}