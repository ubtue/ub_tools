# Extract authority synonyms from translator database and create synonym
# files usable in Solr and Elasticsearch
#!/bin/bash

config_file="/usr/local/var/lib/tuelib/translations.conf"
db=$(inifile_lookup ${config_file} "Database" "sql_username")
db_user=$(inifile_lookup ${config_file} "Database" "sql_username")
db_password=$(inifile_lookup ${config_file} "Database" "sql_password")
outdir="/tmp/synonyms"

mkdir -p ${outdir}

declare -A langmap=(["eng"]="en" ["fre"]="fr" ["ger"]="de" ["gre"]="el"  ["ita"]="it"  ["por"]="pt" ["rus"]="ru" ["spa"]="es")

function GetGermanSynonyms {
    lang='ger'
    sshpass -p ${db_password} mysql --user=${db_user} --password --skip-column-names ${db} <<EOF
    SELECT GROUP_CONCAT(translation) FROM keyword_translations WHERE language_code='${lang}'
    AND gnd_code IN (SELECT gnd_code FROM keyword_translations WHERE language_code='${lang}' AND status='reliable') GROUP BY gnd_code;
EOF
}


function GetSynonymsForLanguage {
   lang=${1}
   sshpass -p "${db_password}" mysql --user="${db_user}" --password --skip-column-names ${db} <<EOF
   SELECT GROUP_CONCAT(translation) FROM keyword_translations WHERE language_code='${lang}' GROUP BY gnd_code;
EOF
}


function GetAllSynonyms {
   sshpass -p "${db_password}" mysql --user="${db_user}" --password --skip-column-names ${db} <<EOF
   SELECT GROUP_CONCAT(translation) FROM keyword_translations GROUP BY gnd_code;
EOF
}


function CleanUp {
   file=${1}
   # Remove specifications that most probably do not occur in fulltext
   sed --in-place --regexp-extended --expression 's/[[:space:]]*<[^>]+>[[:space:]]*//g' $file
   # Remove erroneous newline/tab sequences
   sed --in-place  --regexp-extended --expression 's/(\\n|\\t)+//g' $file
   # Replace synonym separator #
   sed --in-place --regexp-extended --expression 's/#/, /g' $file
}

#German synonyms
outfile_de=${outdir}/synonyms_de.txt
echo "Creating ${outfile_de}"
GetGermanSynonyms > ${outfile_de}


#Synonyms for translated languages
for lang in "eng" "fre" "gre" "ita" "por" "rus" "spa"; do
   outfile=${outdir}/synonyms_${langmap[$lang]}.txt
   echo "Creating ${outfile}"
   GetSynonymsForLanguage $lang > ${outfile}
   CleanUp ${outfile}
done

#All Synonyms
outfile_all="${outdir}/synonyms_all.txt"
echo "Creating ${outfile_all}"
GetAllSynonyms > ${outfile_all}
CleanUp ${outfile_all}




