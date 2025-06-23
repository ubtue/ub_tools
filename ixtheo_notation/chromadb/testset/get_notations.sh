if [[ ! -v SOLR_HOST_AND_PORT ]]; then
    echo "SOLR_HOST_AND_PORT is not set"
    exit 1
fi

curl -s 'http://'${SOLR_HOST_AND_PORT}'/solr/biblio/select?defType=edismax&facet.field=ixtheo_notation&facet.limit=-1&facet.sort=index&facet=true&indent=true&lang=de&q.op=OR&q=*%3A*&useParams=' | jq -r '.facet_counts.facet_fields.ixtheo_notation[]' | sed -n '1p;1~2p'
