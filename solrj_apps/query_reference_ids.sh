#!/bin/bash
#Add reference data terms 
set -o errexit -o nounset


VUFIND_HOME="/usr/local/vufind2"
UBTOOLS_HOME="/usr/local/ub_tools"
SOLR_HOME="$VUFIND_HOME/solr"
SOLRJ_PARENT="$SOLR_HOME/vendor/dist"
SOLRJ_HOME="$SOLRJ_PARENT/solrj-lib"
JETTY_LIB="$SOLR_HOME/vendor/server/solr-webapp/webapp/WEB-INF/lib"
QUERYREF_HOME="$UBTOOLS_HOME/solrj_apps"
EXTLIB="$SOLR_HOME/vendor/server/lib/ext"
#RESOURCES="$SOLR_HOME/vendor/server/resources"
RESOURCES="$VUFIND_HOME/import"
CLASSPATH="$SOLRJ_PARENT/*:$JETTY_LIB/*:$SOLRJ_HOME/*:$EXTLIB/*:$QUERYREF_HOME/*:$RESOURCES/*:./*"

QUERYREF_MAIN=de.unituebingen.ub.ubtools.solrj_apps.QueryRefTermIds

echo $CLASSPATH

if [ $# != 2 ]; then
    echo "usage: $0 reference_list_file output_dir"
    exit 1;
fi

reffile=$1
outputdir=$2

#Query all matching IDs from Solr and write files for each term respectively
cat $reffile | awk --file rewrite_query_expression.awk | xargs -0 --max-args=2 --max-procs=8 java -cp $CLASSPATH  $QUERYREF_MAIN $outputdir

# Create a with with a list of IDs and all matching reference terms
cat $outputir/* | sort -k1 > $outputdir/UNIFIED
awk -F "|" 's != $1 || NR ==1{s=$1;if(p){print p};p=$0;next} {sub($1,"",$0);p=p""$0;}END{print p}' < $outputdir/UNIFIED > $outputdir/MERGED



