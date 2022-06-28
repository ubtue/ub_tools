#!/bin/bash
set -o errexit -o nounset -o pipefail

if [ $# != 1 ]; then
    echo "usage: $0 ptsem_dir"
    exit 1
fi


PTSEMDIR="$1"
RESULTS_SUBDIR="results"
MRC_SUBDIR="mrc"
FINAL_RESULT_FILE_PREFIX="princeton_all_$(date +'%y%m%d')"

#Convert to XML
cd ${PTSEMDIR}
mkdir -p ${RESULTS_SUBDIR}
if [ "$(ls -A ${RESULTS_SUBDIR})" ]; then
    echo "Directory ${PTSEMDIR}/${RESULTS_SUBDIR} not empty - Aborting"
    exit 1
fi
for i in $(ls ptsem*); do 
    /usr/local/ub_tools/cpp/princeton_tools/convert_princeton_archive_to_marcxml.sh \
    ${i} ${RESULTS_SUBDIR}/${i%.zip}.xml; 
done

#Convert to MRC
cd ${RESULTS_SUBDIR}
mkdir -p ${MRC_SUBDIR}
for i in $(ls ptsem*); do 
    marc_convert ${i} mrc/${i%.xml}.mrc; 
done

#Assemble and reconvert
cd ${MRC_SUBDIR}
FINAL_RESULT_FILE_MRC=${FINAL_RESULT_FILE_PREFIX}.mrc
FINAL_RESULT_FILE_XML=${FINAL_RESULT_FILE_PREFIX}.xml
> ${FINAL_RESULT_FILE_MRC}
cat ptsem-* >> ${FINAL_RESULT_FILE_MRC}
marc_convert ${FINAL_RESULT_FILE_MRC} ${FINAL_RESULT_FILE_XML}




#Generate final file

