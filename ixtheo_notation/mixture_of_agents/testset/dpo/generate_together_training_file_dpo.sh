#!/bin/bash

function GetGuessedClasses {
   guessed_classes_dir="$1"
   ppn="$2"
   cat ${guessed_classes_dir}/${ppn}.txt | awk '{ printf "%s", $0 }' 
}

if [ $# != 3 ]; then
    echo "Usage $0 sample_input guessed_classes_dir sample_output"
    exit 1
fi

sample_input="$1"
guessed_classes_dir="$2"
sample_output="$3"

cat ${sample_input} | \
   jq 'del(.[].record.era_facet | .[]? | select(. == "s" or . == "u" or . == "p" or . == "g" or . == "b"))' | \
   jq -s -c '.[][]' |  \
   while read -r obj; do 
        ppn=$(echo "$obj" | jq -r .record.id);
        echo "$obj" | jq --arg GUESSED_CLASSES "$(GetGuessedClasses ${guessed_classes_dir} ${ppn})" '.guessed_classes=$GUESSED_CLASSES';
   done  | jq -s '.' | \
   jq --arg MYPROMPT "$(cat ../prompt.txt)" \
         '.[] |  { "input" : { 
                               messages : [
                                   {role : "system", content : $MYPROMPT },
                                   {role : "user" , content : ("Determine the classes for " + ( .record | tostring))}
                               ]
                    },
                    "preferred_output": [ 
                                            {
                                              "role" : "assistant",
                                              "content" :  ( .correct_answer | tostring) 
                                            }
                                        ],
                    "non_preferred_output" : [ 
                                            {
                                              "role" : "assistant",
                                              "content" : ( .guessed_classes )
                                            }
                                        ]
         }' | jq -s -c '.[]' > ${sample_output}
