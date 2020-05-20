#!/bin/bash
# Extract authority synonyms from translator database and create synonym
# files usable in Solr and Elasticsearch

CONFIG_FILE="/usr/local/var/lib/tuelib/translations.conf"
DB=$(inifile_lookup ${CONFIG_FILE} "Database" "sql_username")
DB_USER=$(inifile_lookup ${CONFIG_FILE} "Database" "sql_username")
DB_PASSWORD=$(inifile_lookup ${CONFIG_FILE} "Database" "sql_password")
OUTDIR="/tmp/synonmyms"
VUFIND_SYNONYM_DIR="/usr/local/vufind/solr/vufind/biblio/conf/synonyms"
ELASTICSEARCH_SYNONYM_DIR="/etc/elasticsarch/synonyms/"


mkdir -p ${OUTDIR}

declare -A langmap=(["eng"]="en" ["fre"]="fr" ["ger"]="de" ["gre"]="el" ["ita"]="it" \
                    ["por"]="pt" ["rus"]="ru" ["spa"]="es" ["hans"]="hans" ["hant"]="hant")

function GetGermanSynonyms {
    sshpass -p ${DB_PASSWORD} mysql --user=${DB_USER} --password --skip-column-names ${DB} <<EOF
    SELECT GROUP_CONCAT(translation SEPARATOR 0x1F) FROM keyword_translations WHERE language_code='ger'
        AND gnd_code IN (SELECT gnd_code FROM keyword_translations WHERE language_code='ger' AND status='reliable') GROUP BY gnd_code;
EOF
}


function GetSynonymsForLanguage {
   lang=${1}
   sshpass -p "${DB_PASSWORD}" mysql --user="${DB_USER}" --password --skip-column-names ${DB} <<EOF
   SELECT GROUP_CONCAT(translation SEPARATOR 0x1F) FROM keyword_translations WHERE language_code='${lang}' GROUP BY gnd_code;
EOF
}


function GetAllSynonyms {
   sshpass -p "${DB_PASSWORD}" mysql --user="${DB_USER}" --password --skip-column-names ${DB} <<EOF
   SELECT GROUP_CONCAT(translation SEPARATOR 0x1F) FROM keyword_translations GROUP BY gnd_code;
EOF
}


function CleanUp {
   file=${1}
   # Escape comma in original data
   sed --in-place --regexp-extended --expression 's/,/\\,/g' $file
   # Replace field separator (0x1F) by comma
   sed --in-place --regexp-extended --expression 's/[\x1F]/,/g' $file
   # Remove specifications that most probably do not occur in fulltext
   sed --in-place --regexp-extended --expression 's/[[:space:]]*<[^>]+>[[:space:]]*//g' $file
   # Remove erroneous newline/tab sequences
   sed --in-place  --regexp-extended --expression 's/(\\n|\\t)+//g' $file
   # Replace synonym separator #
   sed --in-place --regexp-extended --expression 's/#/, /g' $file
   sed --in-place --regexp-extended --expression 's/,[[:space:]]*,/,/' $file # Remove results of erroneous synonym separators at the end (1)
   sed --in-place --regexp-extended --expression 's/,[[:space:]]*$//' $file # Remove results of erroneous synonym separators at the end (2)
   # Replace in invalid placeholders
   sed --in-place --regexp-extended --expression 's/,?[[:space:]]*\?\?\?[[:space:]]*(,)?/\1/g' $file
}


#German synonyms
OUTFILE_DE=${OUTDIR}/synonyms_de.txt
echo "Creating ${OUTFILE_DE}"
GetGermanSynonyms > ${OUTFILE_DE}
CleanUp ${OUTFILE_DE}


#Synonyms for translated languages
for lang in "eng" "fre" "gre" "ita" "por" "rus" "spa" "hans" "hant"; do
   OUTFILE=${OUTDIR}/synonyms_${langmap[$lang]}.txt
   echo "Creating ${OUTFILE}"
   GetSynonymsForLanguage $lang > ${OUTFILE}
   CleanUp ${OUTFILE}
done

#All Synonyms
OUTFILE_ALL="${OUTDIR}/synonyms_all.txt"
echo "Creating ${OUTFILE_ALL}"
GetAllSynonyms > ${OUTFILE_ALL}
CleanUp ${OUTFILE_ALL}

if [ -d ${VUFIND_SYNONYM_DIR} ]; then
    cp -av ${OUTDIR}/synonyms_*.txt ${VUFIND_SYNONYM_DIR}
fi

if [ -d ${ELASTICSEARCH_SYNONYM_DIR} ]; then
     cp -av ${OUTDIR}/synonyms_*.txt ${ELASTICSEARCH_SYNONYM_DIR}
fi
