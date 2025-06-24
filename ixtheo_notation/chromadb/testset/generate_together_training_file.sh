#!/bin/bash

if [ $# != 2 ]; then
    echo "Usage $0 sample_input sample_output"
    exit 1
fi

sample_input="$1"
sample_output="$2"

cat ${sample_input} | \
   jq 'del(.[].record.era_facet | .[]? | select(. == "s" or . == "u" or . == "p" or . == "g"))' |  \
   jq --arg MYPROMPT "$(cat prompt.txt)" \
         '.[] |  { messages : [
                    {role : "system", content : $MYPROMPT },
                    {role : "user" , content : ("Determine the classes for " + ( .record | tostring))},
                    {role : "assistant", content : ( .correct_answer | tostring)}
                ]
         }' | \
jq -s -c '.[0:1000] | .[]' > ${sample_output}
