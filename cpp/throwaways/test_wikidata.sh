#!/bin/bash
i=0
gnds=($( sudo mysql ixtheo -B -e 'SELECT DISTINCT gnd_code FROM keyword_translations LIMIT 10' | tail -n +2))
gnds_length=${#gnds[@]}
echo "Length: ${#gnds[@]}"
while :
   gnd=${gnds[(($i % gnds_length))]}
   echo "GND: ${gnd}"
   do
   /usr/local/ub_tools/cpp/test/download_test 'https://query.wikidata.org/sparql?query=PREFIX%20schema%3A%20%3Chttp%3A%2F%2Fschema.org%2F%3E%0ASELECT%20DISTINCT%20%3Fitem%20%3Ftitle%20%3Flang%20%20WHERE%20%7B%0A%20%20%20%20%3Fitem%20wdt%3AP227%20%22'${gnd}'%22%20.%0A%20%20%20%20%20%20%5B%20schema%3Aabout%20%3Fitem%20%3B%20schema%3Aname%20%3Ftitle%3B%20schema%3AinLanguage%20%3Flang%3B%20%5D%20.%0A%20%20%20%20FILTER%20(%3Flang%20IN%20(%22en%22%2C%22de%22%2C%22fr%22%2C%22ru%22%2C%22pl%22%2C%22es%22%2C%22pt%22%2C%22it%22%2C%22gr%22))%0A%7D%0A' /dev/null
   i=$(($i+1))
   echo $i
done
