#!/bin/bash
set -o errexit -o nounset

EXTRACT_DIR="extracted"

function ExitHandler {
    (setsid kill -- -$$) &
    exit 1
}
trap ExitHandler SIGINT


function ConvertXMLsInDirectoryToText {
    cd $1
    mkdir --parents $EXTRACT_DIR
    find . -name '*.xml' | while read f;
    do
        journal_publishing_processor --force-ocr $f ${EXTRACT_DIR}/${f%.*}.txt &
        # Trick to limit number of parallel thread to number of CPU cores
        # Disable abort on error
        set +e
        [ $( jobs | wc -l ) -ge $( nproc ) ] && wait
        set -e
    done
    wait
    cd -
}


function GetFulltextDirectories {
    echo $(find . -mindepth 1 -maxdepth 1 -type d)
}


if [ $# != 1 ]; then
    echo "usage: $0 brill_root_dir"
    exit
fi


root_dir=$1
cd $root_dir
directories=$(GetFulltextDirectories)
for directory in $directories
do
   echo "Converting $directory..."
   ConvertXMLsInDirectoryToText $directory
done


