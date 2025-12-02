#!/bin/bash
set -o errexit -o nounset -o pipefail

trap RemoveTempFiles EXIT

pass_file=$(mktemp -u -t pass_fileXXXX.mrc)
out_file=$(mktemp -u -t out_fileXXXX.mrc)

function RemoveTempFiles {
    [ -e ${pass_file} ] && rm ${pass_file}
    [ -e ${out_file} ] && rm ${out_file}
}

function Usage() {
  echo "Usage: $0 solr_server ppn"
  exit 1;
}

if [ $# -lt 2 ]; then
    Usage
fi

omit_local_data=0
if [ "$1" == "--omit-local-data" ]; then
   omit_local_data=1
   shift
fi

if [ $# != 2 ]; then
    Usage
fi

solr_server="$1"
ppn="$2"
request="http://$solr_server:8983/solr/biblio/select?fl=fullrecord&indent=true&q.op=OR&q=id%3A$ppn&useParams="

set +o errexit
solr_record=$(curl -s ${request})

if [ $? -ne 0 ]; then
    echo "Request ${request} failed: ${solr_record}"
    exit 1
fi
set -o errexit

num_found=$(echo ${solr_record} | jq .response.numFound)
if [ "$num_found" -eq 0 ]; then
    echo "No results found for PPN ${ppn}"
    exit 1
fi

set +o errexit
marc_record=$(marc_grep --input-format=marc-21 <(yaz-marcdump -i json -o marc \
    <(echo ${solr_record} | jq -r .response.docs[].fullrecord)) \
    'if "001"==".*" extract *' marc_binary)

if [ $? -ne 0 ]; then
    echo "Error converting record $ppn"
    exit 1
fi
set -o errexit

if [ $omit_local_data -eq 1 ]; then
    make_named_pipe ${pass_file}
    (printf '%s' "${marc_record}" > ${pass_file}) &
    ln -s /dev/stdout ${out_file}
    marc_filter ${pass_file} ${out_file} --remove-fields 'LOK:.*'
else
    printf '%s' "${marc_record}"
fi
