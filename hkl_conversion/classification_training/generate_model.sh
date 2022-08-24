#!/bin/bash
set -o errexit

python create_data.py
python -m spacy init fill-config ./base_config.cfg ./config.cfg
python -m spacy train config.cfg --output ../training/output

