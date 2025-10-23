#!/bin/bash
set -o errexit -o nounset -o pipefail


if [ $# != 1 ]; then
   echo "Usage $0 marcxml"
fi

INPUT_XML="$1"

function setupStdoutXml() {
    stdout_xml=$(mktemp --suffix=.xml --dry-run)
    ln --symbolic /dev/stdout ${stdout_xml}
    trap 'rm -f ${stdout_xml}' EXIT
    STDOUT_XML=${stdout_xml}
}


STDOUT_XML=""
setupStdoutXml

marc_filter "$INPUT_XML" "${STDOUT_XML}" --remove-fields '935a:tiep' | sponge "$INPUT_XML"
