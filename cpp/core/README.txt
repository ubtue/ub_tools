Generate queries needed to download records relevant for criminology from the CORE database
generate_core_query.sh generates a numbered list of batch queries in the current directory, i.e. e.g. call "generate_core_query.sh bulk_query.json" in an empty subdirectory "bulk".

download_core_bulk.sh can use the generated files to process the partial downloands, i.e. e.g "download_core_bulk.sh bulk bulk_download.json".
A correct CORE API key must be provided.
