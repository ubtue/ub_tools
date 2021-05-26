To install, first create the directory:
- mkdir /usr/local/es_backup
- chown elasticsearch:elasticsearch /usr/local/es_backup

Also register the directory in /etc/elasticsearch/elasticsearch.yml:
- path.repo: ["/usr/local/es_backup"]

Elasticsearch restart might be necessary:
- systemctl restart elasticsearch

Register the snapshot repository in elasticsearch:
- curl -H 'Content-Type: application/json' -T repository.json http://localhost:9200/_snapshot/es_backup

Register the policy (cronjob+indexes) in elasticsearch:
- curl -H 'Content-Type: application/json' -T policy.json http://localhost:9200/_slm/policy/es_backup


For additional information, have a look at the manual:
- https://www.elastic.co/guide/en/elasticsearch/reference/current/snapshots-register-repository.html
- https://www.elastic.co/guide/en/elasticsearch/reference/current/snapshot-lifecycle-management-api.html

Additional useful commands:
- Show existing indices:               wget http://localhost:9200/_cat/indices
- Show existing snapshot repositories: wget http://localhost:9200/_snapshot
- Show existing snapshots:             wget http://localhost:9200/_cat/snapshots/es_backup
- Show existing policies:              wget http://localhost:9200/_slm/policy
- Execute snapshot manually:           curl -X PUT http://localhost:9200/_slm/policy/es_backup/_execute
- Execute restore manually:            curl -X POST http://localhost:9200/_snapshot/es_backup/fulltext-snap-210526-hi3kbl4ntk2pq2jbo6cpxw/_restore
