#!/bin/bash
# Create krimdok local keyword file from the original output and make misspellings and potential GND references explicit


if [ $# != 4 ]; then
    echo "usage: $0 A-Z_output A_Z_bereinigt gnd_full.mrc result"
    exit 1
fi

readonly AZ_OUTPUT=$1
readonly AZ_CLEANED=$2
readonly GND_FULL=$3
readonly RESULT_FILE=$4
readonly tmp_prefix=$(basename $0)

function MkTempFile {
   echo $(mktemp /tmp/${tmp_prefix}.XXXXX)
}

readonly gnd_matched=$(MkTempFile)
readonly gnd_unmatched=$(MkTempFile)
readonly gnd_matched_clean=$(MkTempFile)
readonly association_and_spelling=$(MkTempFile)


function GenerateKeywordToGNDMap {
   single_marc_convert_keywords ${GND_FULL} ${AZ_CLEANED} ${gnd_matched} ${gnd_unmatched} 
   cat ${gnd_matched} | sed -r 's/^"|"$//g' |  sed 's/","/;/' > ${gnd_matched_clean}
}


function ReorderAZOutput {
     readonly az_output_reordered=$(MkTempFile)
     # Remove ';' at the end and normalize \r\n
     cat ${AZ_OUTPUT} | sed ':a;N;$!ba;s/;\r\n/\n/g' | sed '$ s/.$//' | \
     `# Resort columns to get final form in first column` \
     tr -d '\r' |  awk -F';' '{ 
          printf("%s", $NF); 
          for (i = 1; i < NF; i++) { 
              printf(";%s", $i); 
          } 
          print ""; 
     }' > ${az_output_reordered} 
}


function JoinAssociationAndSpelling {
    join -t';' -1 1 -2 1  -a 2 <(sort ${gnd_matched_clean}) <(sort ${az_output_reordered}) > ${association_and_spelling}
}


function MergeDuplicatesInAssociationAndSpelling {
   #This is the two file processing pattern c.f. https://backreference.org/2010/02/10/idiomatic-awk/
   awk -F';' ' NR==FNR { 
       if ($1==prev) { 
            mergeone = ""
            offset = 2;
            if ($2 ~ /[0-9X]{9,10}/) {
                offset=5;
            } 
            for (i = offset; i <= NF; ++i) {
                mergeone = mergeone ";" $i;
            }
            merged[$1] = merged[$1] mergeone; 
        } 
        prev=$1;
        next;
    }
    {  
        if ($1!=prev) { 
            printf("%s",$1);
            if ($2 ~ /[0-9X]{9,10}/) {
                printf(";%s;%s;%s", $2, $3, $4);
            }
            printf("%s\n", merged[$1]);
        }
        prev=$1;
    }
    ' \
    ${association_and_spelling} ${association_and_spelling}
}


function CleanUp {
   rm /tmp/${tmp_prefix}.?????
}

GenerateKeywordToGNDMap
ReorderAZOutput
JoinAssociationAndSpelling
MergeDuplicatesInAssociationAndSpelling > ${RESULT_FILE}
CleanUp
