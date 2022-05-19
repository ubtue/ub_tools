#!/bin/bash
set -o errexit -o nounset -o pipefail

if [ $# != 2 ]; then
    echo "usage: $0 ptsem-XX.zip output.mrc"
    exit 1
fi


archive_file="$1"
output_file="$2"
archive_contents=$(7z l ${archive_file} | grep 'xml$' | awk '{ print $6 }')
TOOL_BASE_PATH="/usr/local/ub_tools/cpp/princeton_tools"
XSLT_FILE="MODS3-7_MARC21slim_XSLT1-0.xsl"
PPN_BASE=$(echo ${archive_file} | sed -r -e 's/ptsem-([[:digit:]]+)\.zip/\1/')
CLASSPATH="${TOOL_BASE_PATH}/marc4j/marc4j.jar:${TOOL_BASE_PATH}/marc4j/marcxml.jar"
TMP_FILE="/tmp/p_tmp"
FIFO_FILE="/tmp/p_fifo"
if [[ ! -p ${FIFO_FILE} ]]; then
    mkfifo ${FIFO_FILE}
fi

trap "rm -f ${FIFO_FILE}; rm -f ${TMP_FILE}" EXIT

> ${output_file}
i=0
for content in ${archive_contents}; do
     i=$((i+1))
     ppn=$(printf "${PPN_BASE}_%06d" ${i})
     echo ${ppn}
     7z x -so ${archive_file} ${content} | \
         xmlstarlet tr ${TOOL_BASE_PATH}/${XSLT_FILE} | \
         xmlstarlet ed -s /marc:record -t elem -n 'marc:controlfield' -v "${ppn}"  \
                    --var new_node '$prev' -i '$new_node' -t attr -n "tag" -v "001" | \
         sed -r -e 's/(<[/]?)marc:/\1/g' | \
         sed '1d' | sed -e '1i <?xml version="1.0" encoding="UTF-8"?><collection>' | sed -e'$a </collection>' > ${TMP_FILE}
     java -cp ${CLASSPATH} gov.loc.marcxml.MARC21slim2MARC ${TMP_FILE} /dev/stdout | sponge -a ${output_file}
     if [[ ${i} -gt 1000 ]]; then
         exit 0
     fi
done
