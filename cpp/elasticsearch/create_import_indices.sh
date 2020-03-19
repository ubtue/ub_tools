#!/bin/bash
# Address the problem  of lacking "DELETE without COMMIT" in elasticsearch:
# Create temporary indices and alias them to enable importing from scratch 
# while keeping read access to the old index.
# Notice that you might obtain duplicate results during indexing that must be handled on client side


host_and_port=$(inifile_lookup /usr/local/var/lib/tuelib/Elasticsearch.conf Elasticsearch host)


function selectExisting {
    for schema in *_schema.json; do
        index="${schema%_schema.json}"
        http_code=$(curl -s --silent --output /dev/null --head --write "%{http_code}" ${host_and_port}/${index})
        if [ ${http_code} == "200" ]; then
            existing_indices+=("${index}")
        fi
     done

}
   

existing_indices=()
selectExisting
for index in ${existing_indices[@]}; do
   echo $index "present"
   curl --request GET "${host_and_port}/_cat/aliases/${index}" --header 'Content-Type: application/json' 
done


   #curl -XPUT "${host_and_port}/${index}/_settings" --header 'Content-Type: application/json' --data'{ "settings": { "index.blocks.write": true } }'
