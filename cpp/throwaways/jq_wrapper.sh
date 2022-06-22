#!/bin/bash
# To be used with krim_keywords_add_gnd_type.sh

echo "$@" | jq -cjr '.type, ";",  .gndSubjectCategory[0].label'
