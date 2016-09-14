#!/bin/bash
#Add reference data terms 
set -o errexit -o nounset


VUFIND_HOME="/usr/local/vufind2"
SOLR_HOME="$VUFIND_HOME/solr"
SOLRJ_PARENT="$SOLR_HOME/vendor/dist"
SOLRJ_HOME="$SOLRJ_PARENT/solrj-lib"
JETTY_LIB="$SOLR_HOME/vendor/server/solr-webapp/webapp/WEB-INF/lib"
TEMP_SOLRTEST="/home/qubhw01/solrjtest"
EXTLIB="$SOLR_HOME/vendor/server/lib/"
CLASSPATH="$SOLRJ_PARENT/*:$JETTY_LIB/*:$SOLRJ_HOME/*:$EXTLIB/*:$TEMP_SOLRTEST/*:."

echo $CLASSPATH

if [ $# != 2 ]; then
    echo "usage: $0 reference_list_file output_dir"
    exit 1;
fi

reffile=$1
outputdir=$2

cat $reffile | awk --file rewrite_query_expression.awk | xargs -0 --max-args=2 --max-procs=20 java -cp $CLASSPATH SolrJTest $outputdir
