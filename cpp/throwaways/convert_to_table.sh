#!/bin/bash
set -o nounset 

function RemoveTempFiles {
    rm ${tmpfile1} ${tmpfile2}
}

tmpfile1=$(mktemp -t engtransXXXXX.txt)
tmpfile2=$(mktemp -t engtransXXXXX.txt)

read -r -d '' "query_gnd" <<- EOF
  echo -n "\$@;"
  sudo mysql ixtheo -B -e \
  'SELECT DISTINCT gnd_code FROM keyword_translations WHERE status='\''reliable'\'' AND language_code='\''ger'\'' AND translation='\'''"\$@"''\''' | \
  grep -v "gnd_code" | paste -sd "," -  
EOF

#Do some cleanup and generate the associations
cat "$1" | grep -v '^$' | grep -v "^Mail vom" | sed -re 's/\s*=\s*/=/g '| awk -F= '{printf "%s;%s;\n", $1, $2}' \
    > ${tmpfile1}

#Do the lookup and join the results
paste ${tmpfile1} <(cat ${tmpfile1} | awk -F';' '{print $1}' | sed -re 's/\s*[<].*[>]//' | xargs -I'{}' \
    bash -c "${query_gnd}" _ '{}') > ${tmpfile2}

cat ${tmpfile2} | awk -F';' '{printf "%s;%s;%s\n", $1,$2,$4}'
