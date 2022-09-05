#!/bin/bash
set -o errexit -o nounset

mypath=$(readlink -f "${BASH_SOURCE:-$0}")
mydir=$(dirname ${mypath})
olddir=$(pwd)
cd ${mydir}

annotations_dir="annotations"
training_dir="training"
converted_dir="${training_dir}/converted"
cumulated_file="${annotations_dir}/ner_all.conll"
PATH=".:${PATH}"

# Merge individual files to one
> ${cumulated_file}
fix_label_studio_conll_files.sh "${annotations_dir}"
for file in $(ls -1 --directory "${annotations_dir}"/*); do
    if [[ "${file}" == "${cumulated_file}" ]]; then
        continue;
    fi
    echo ${file}
    cat ${file} >> ${cumulated_file}
done

# Convert to individual documents
sed -i -r -e 's/^$/-DOCSTART- -X- O O/' "${cumulated_file}"

#python -m spacy convert -s -b "../senter_training/training/output/model-best" -n 0 "${cumulated_file}" "${converted_dir}" -s
python -m spacy convert  -n 0 "${cumulated_file}" "${converted_dir}" -s
# Generate train and dev data
create_ner_training.py

python -m spacy init fill-config ./base_config.cfg ./config.cfg
python -m spacy train config.cfg --output "${training_dir}/output" --paths.train "${training_dir}/ner_train.spacy" --paths.dev "${training_dir}/ner_valid.spacy"

cd ${olddir}
