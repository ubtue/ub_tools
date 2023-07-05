#!/bin/bash
# Factored out run for collect_journal_stats
set -o errexit -o nounset

source pipeline_functions.sh

function Usage {
    echo "usage: $0 ixtheo|krimdok"
    exit 1
}

if [ $# != 1 ]; then
    Usage
fi

system_type="$1"
if [[  ${system_type} != ixtheo && ${system_type} != krimdok ]]; then
    Usage
fi

# Sets up the log file:
logdir=/usr/local/var/log/tuefind
log="${logdir}/collect_journal_stats.log"
rm -f "${log}"

title_file=$(ls -1 GesamtTiteldaten-post-pipeline-* | sort --reverse | head --lines 1)
if [[ ! "${title_file}" =~ GesamtTiteldaten-post-pipeline-[0-9][0-9][0-9][0-9][0-9][0-9].mrc ]]; then
    echo 'Could not identify a file matching GesamtTiteldaten-post-pipeline-[0-9][0-9][0-9][0-9][0-9][0-9].mrc!'
    exit 1
fi

json_out_file="/tmp/collect_journal_stats.json"
rm -f "${json_out_file}"

# Determines the embedded date of the files we're processing:
date=$(DetermineDateFromFilename ${title_file})

echo "Using \"${title_file}\" as input for ${system_type}"
OVERALL_START=$(date +%s.%N)

# Note: It is necessary to run this phase after articles have had their journal's PPN's inserted!
echo "Collect Journal Stats for Zeder"
collect_journal_stats ${system_type} ${title_file} ${json_out_file}

echo "Uploading generated JSON file to Zeder..."
curl --verbose --request POST --header "Content-Type: multipart/form-data" --form Datenquelle=$(hostname) --form "Datei=@${json_out_file}" --form "s_stufe=2" "http://www-ub.ub.uni-tuebingen.de/zeder/cgi-bin/index.cgi/artikelliste_hochladen"

echo "Processing finished after $(CalculateTimeDifference $OVERALL_START $(date +%s.%N)) minutes."
