#!/bin/bash
set -o errexit -o nounset

mypath=$(readlink -f "${BASH_SOURCE:-$0}")
mydir=$(dirname ${mypath})
olddir=$(pwd)
cd ${mydir}

annotations_dir="annotations"
training_dir="training"
PATH=".:${PATH}"
python -m spacy init fill-config ./base_config.cfg ./config.cfg
fix_label_studio_conll_files.sh "${annotations_dir}"
for file in $(ls --directory -1 "${annotations_dir}"); do
   python -m spacy convert "${file}" "${training_dir}"
   python -m spacy train config.cfg --output "${training_dir}/output" --paths.train training/ --paths.dev training/ 
done

cd ${olddir}
