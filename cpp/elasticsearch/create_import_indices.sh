#!/bin/bash
# Address the problem  of lacking "DELETE without COMMIT" in elasticsearch:
# Create temporary indices and alias them to enable importing from scratch
# while keeping read access to the old index.
# Notice that you might obtain duplicate results during indexing that must be handled on client side
set -o errexit -o nounset


host_and_port=$(inifile_lookup /usr/local/var/lib/tuelib/Elasticsearch.conf Elasticsearch host)


function GetExistingFulltextIndices {
    existing_fulltext_indices=()
    for schema in *_schema.json; do
        index="${schema%_schema.json}"
        http_code=$(curl --fail --silent --output /dev/null --head --write "%{http_code}" ${host_and_port}/${index})
        if [ ${http_code} == "200" ]; then
            existing_fulltext_indices+=("${index}")
        else
            echo "Index ${index} not present -- Aborting"
            exit 1
        fi
    done
    echo ${existing_fulltext_indices[@]}
}


function GetExistingFulltextAliases {
    existing_fulltext_indices=$@
    check_output=()
    for index in ${existing_fulltext_indices[@]}; do
        check_output+=($(curl --fail --silent --request GET "${host_and_port}/_cat/aliases/${index}" --header 'Content-Type: application/json'))
    done
    echo ${check_output[@]}
}


function CreateReadWriteIndicesAndAliases {
    indices=$@
    for index in ${indices[@]}; do
        curl --fail --silent --request PUT "${host_and_port}/${index}/_settings" --header 'Content-Type: application/json'  \
            --data '{ "settings": { "index.blocks.write": true } }'

        curl --fail --silent --request POST "${host_and_port}/${index}/_clone/${index}_read" --header 'Content-Type: application/json'
        curl --fail --silent --request GET "${host_and_port}/_cluster/health/${index}_read?wait_for_status=yellow&timeout=30s" --header 'Content-Type: application/json'

        curl --fail --silent --request POST "${host_and_port}/${index}/_clone/${index}_write" --header 'Content-Type: application/json'
        curl --fail --silent --request GET "${host_and_port}/_cluster/health/${index}_write?wait_for_status=yellow&timeout=30s" --header 'Content-Type: application/json'
        curl --fail --silent --request PUT "${host_and_port}/${index}_write/_settings" --header 'Content-Type: application/json'  \
            --data '{ "settings": { "index.blocks.write": null } }'
        curl --fail --silent --request POST "${host_and_port}/_aliases" --header 'Content-Type: application/json' --data-binary @- << ENDJSON
                       { "actions" :
                          [
                             { "remove_index" : { "index" : "${index}" } },
                             { "add" : { "index" : "${index}_write",
                                         "alias" : "${index}",
                                         "is_write_index" : true
                                       }
                             },
                             { "add" : { "index" : "${index}_read",
                                         "alias" : "${index}"
                                       }
                             }
                          ]
                       }
ENDJSON
    done
}

#Check whether all necessary indices exist = Abort if not
existing_fulltext_indices=$(GetExistingFulltextIndices)


#Check whether aliases exist => Abort if they do
existing_fulltext_aliases=($(GetExistingFulltextAliases ${existing_fulltext_indices[@]}))
if [ ${#existing_fulltext_aliases[@]} -ne 0 ]; then
    echo "Don't know how to handle previously existing aliases (${#existing_fulltext_aliases[@]})"
    exit 1
fi

#Clone indices to a temporary name, set the aliases and select the write alias
CreateReadWriteIndicesAndAliases ${existing_fulltext_indices[@]}
