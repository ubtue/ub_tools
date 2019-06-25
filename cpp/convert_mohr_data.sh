#!/bin/bash
set -o errexit -o nounset

EXTRACT_DIR="./extracted"

function ExitHandler {
    (setsid kill -- -$$) &
    exit 1
}
trap ExitHandler SIGINT


function ConvertPDFsInDirectoryToText {
    cd $1
    mkdir --parents $EXTRACT_DIR
    find . -name '*.pdf' | while read f;
    do
        jobs_total=$( jobs | wc -l )
        nprocs=$(nproc)
        echo "Processing $f with jobs_total $jobs_total and $nprocs"
        embedded_metadata_pdf_processor --output-dir=${EXTRACT_DIR} $f &
        # Trick to limit number of parallel thread to number of CPU cores
        # Disable abort on error
        set +e
        [ $( jobs | wc -l ) -ge $( nproc ) ] && wait
        set -e
    done
    wait
    cd -
}

function FindUnconvertedFiles {
    cd $1
    find . -name '*.pdf' | while read f;
    do
        if [ ! -e $EXTRACT_DIR/${f%.pdf}.txt ]; then
            echo "$f was not converted"
        fi
    done
    cd -
}

if [ $# != 1 ]; then
    echo "usage: $0 mohr_root_dir"
    exit
fi


root_dir=$1
cd ${root_dir}
echo "Converting ${root_dir}..."
ConvertPDFsInDirectoryToText .
echo "Find Unconverted"
FindUnconvertedFiles .

