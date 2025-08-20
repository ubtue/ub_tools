#/bin/bash
set -o errexit -o nounset
# Use creation of import indices to setup new scheme by removing write
# index then setup the new schema and reindex from the read index

function Usage() {
   echo "$0: Must be called with both full_text_cache_schema.json and full_text_cache_html_schema.json present in the current directory"
   exit 1
}

# Safety check...
indices=("full_text_cache" "full_text_cache_html")
for index in ${indices[@]}; do
    if [ ! -f ${index}_schema.json ]; then
        Usage
    fi
done

# Here we go...
create_import_indices.sh

host_and_port=$(inifile_lookup /usr/local/var/lib/tuelib/Elasticsearch.conf Elasticsearch host)

for index in ${indices[@]}
do
     printf "\nDelete current write index for ${index}\n"
     curl --fail --request DELETE "${host_and_port}/${index}_write/"
     schema="${index}_schema.json"
     printf "\nImport new mappings\n"
     curl --fail --request PUT --header 'Content-Type: application/json' "${host_and_port}/${index}_write" --data @"${schema}"
     printf "\nReindex from the read index for ${index}\n"
     curl --fail --request POST "${host_and_port}/_reindex" -H 'Content-Type: application/json' -d'{  "source": { "index": "'"${index}"'_read" },  "dest": { "index": "'"${index}"'_write" }}'
done

remove_import_aliases.sh
printf "\nFinished setting up new mappings...\n\n"
