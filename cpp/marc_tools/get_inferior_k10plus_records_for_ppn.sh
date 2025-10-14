#!/bin/bash
set -o errexit -o nounset -o pipefail

K10PLUS_SUPERIOR_QUERY='https://sru.k10plus.de/opac-de-627?version=1.1&operation=searchRetrieve&query=pica.1049%3DSUPERIOR_PPN+and+pica.1045%3Drel-nt+and+pica.1001%3Db&maximumRecords=10000&recordSchema=marcxml'

if [ $# != 2 ]; then
   echo "Usage $0 superior_ppn outfile.xml"
   exit 1;
fi

superior_ppn="$1"
outfile="$2"

sru_query="${K10PLUS_SUPERIOR_QUERY/SUPERIOR_PPN/$superior_ppn}"
curl --silent "${sru_query}" | saxonb-xslt -s:- -xsl:sru_to_marc.xslt -o:"${outfile}"
