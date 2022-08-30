#!/bin/bash
set -o errexit

mypath=$(readlink -f "${BASH_SOURCE:-$0}")
mydir=$(dirname ${mypath})
olddir=$(pwd)
cd ${mydir}
python create_senter_data.py
python -m spacy init fill-config ./base_config1.cfg ./config.cfg
#python -m spacy init fill-config ./base_config_transformer.cfg ./config.cfg
python -m spacy train config.cfg --output training/output
cd ${olddir}
