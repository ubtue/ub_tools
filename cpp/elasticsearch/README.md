##Remarks for Elasticsearch Index fixing

### Avoiding/Removing Replica Shards
Elasticsearch supposes a multi-node (=multi computer) where the shards (=a self contained index) are distributed between the nodes. There are primary and replica shards. However, we currently run it as single-node setup (c.f. `discovery.type: single-node` in elasticsearch.yml). Fro a proper setup and a green status (`curl -XGET 'localhost:9200/_cluster/health?pretty'`) unassigned shards can be avoided be explicitly disabling replica shards.

For our indices this can be achieved e.g. by

```curl -XPUT -H 'Content-Type: application/json'  'localhost:9200/full_text_cache_html/_settings' -d '{ "index" : { "number_of_replicas" : 0 } }'```.

To avoid the generation of replicas (e.g. for the log indices or by reindexing of existing records, a template mechanism can be used:

```curl -XPUT 'localhost:9200/_index_template/my_logs_template'  -H 'Content-Type: application/json'  -d '{ "index_patterns" : ["ds-.logs-deprecation*"], "template" : { "settings" : { "number_of_replicas" : 0 } } }'```


```curl -XPUT 'localhost:9200/_index_template/full_text_cache_template'  -H 'Content-Type: application/json'  -d '{ "index_patterns" : ["full_text_cache*"], "template" : { "settings" : { "number_of_replicas" : 0 } } }'```


### Disable Geoip
To avoid errors in elasticsearch.log on startup add `ingest.geoip.downloader.enabled: false` in elaticsearch.yml.















