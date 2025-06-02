#!/bin/bash
# Convert the alias setup created by create_import_indices.sh back to a single index per aspect
# Make sure the import of each write index is finished
set -o errexit -o nounset

host_and_port=$(inifile_lookup /usr/local/var/lib/tuelib/Elasticsearch.conf Elasticsearch host)

function GetFulltextReadWriteIndices {
    fulltext_read_write_indices=()
    for schema in *_schema.json; do
        base_index="${schema%_schema.json}"
        fulltext_read_write_indices+=(${base_index}_read ${base_index}_write)
    done

    existing_read_write_indices=()
    for index in ${fulltext_read_write_indices[@]}; do
        http_code=$(curl --fail --silent --output /dev/null --head --write "%{http_code}" ${host_and_port}/${index})
        if [ ${http_code} == "200" ]; then
            existing_read_write_indices+=("${index}")
        else
            echo "Index ${index} not present -- Aborting"
            exit 1
        fi
    done
    echo ${existing_read_write_indices[@]}
}


function RemoveAliases {
    fulltext_read_write_indices=$@
    for index in ${fulltext_read_write_indices[@]}; do
        curl --fail --silent --request POST "${host_and_port}/_aliases" --header 'Content-Type: application/json' --data-binary @- << ENDJSON
                  { "actions" :
                     [
                        { "remove" : { "index" : "${index}",
                                       "alias" : "${index%_*}"
                                     }
                        },
                        { "remove" : { "index" : "${index}",
                                       "alias" : "${index%_.*}}"
                                     }
                        }
                     ]
                  }
ENDJSON
    done
}

function RenameWriteIndices {
    fulltext_read_write_indices=$@
    for index in ${fulltext_read_write_indices[@]}; do
        if [[ ${index} =~ ^.*_write$ ]]; then
            curl --fail --silent --request PUT "${host_and_port}/${index}/_settings" --header 'Content-Type: application/json'  \
            --data '{ "settings": { "index.blocks.write": true } }'
            curl --fail --silent --request POST "${host_and_port}/${index}/_clone/${index%_write}" --header 'Content-Type: application/json'
            curl --fail --silent --request GET "${host_and_port}/_cluster/health/${index%_write}?wait_for_status=yellow&timeout=30s" --header 'Content-Type: application/json'
            curl --fail --silent --request PUT "${host_and_port}/${index%_write}/_settings" --header 'Content-Type: application/json'  \
            --data '{ "settings": { "index.blocks.write": null } }'
        fi
    done
}


function DeleteImportIndices {
    fulltext_read_write_indices=$@
    for index in ${fulltext_read_write_indices[@]}; do
        curl --fail --silent --request DELETE "${host_and_port}/${index}"
    done
}


fulltext_read_write_indices=$(GetFulltextReadWriteIndices)
RemoveAliases ${fulltext_read_write_indices[@]}
RenameWriteIndices ${fulltext_read_write_indices[@]}
DeleteImportIndices ${fulltext_read_write_indices[@]}
echo "Done..."
