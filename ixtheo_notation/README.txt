Tools in the context of automatic assignment of IxTheo Notations:

Stages:

1.) Download fulltexts from ES: download_fulltexts_from_es.sh
2.) Augment the data with information form solr (title, keywords, existing
    IxTheo-Notation): augment_with_solr_information.py [If inpiut is not an array, use <(cat /tmp/tocs_240605.json | jq -s) as input file]
3.) Generate new keywords based on title, fulltext and keywords:
    generate_new_keywords.py
4.) Generate new notations: generate_new_notations.py
