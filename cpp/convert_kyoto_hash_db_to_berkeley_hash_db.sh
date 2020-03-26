#!/bin/bash
set -o errexit -o nounset


if [ $# != 1 ]; then
    echo Usage: database_to_convert
    exit 1
fi


if [ ${1: -3} != ".db" ]; then
    echo "Datbase names must end in .db"
    exit 2
fi


mv -i "$1" "${1::-2}"kdb
convert_kyoto_hash_db_to_berkeley_hash_db "${1::-2}"kdb "$1"
