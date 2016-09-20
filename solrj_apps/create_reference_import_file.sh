#!/bin/bash
#Add reference data terms 
set -o errexit -o nounset

if [ $# != 2 ]; then
    echo "usage: $0 Hinweissätze-YYMMDD.txt output_dir"
    exit 1;
fi

if [[ ! "$1" =~ Hinweissätze-[0-9][0-9][0-9][0-9][0-9][0-9].txt ]]; then
    echo "Hinweissatzdatei entspricht nicht dem Muster Hinweissätze-[0-9][0-9][0-9][0-9][0-9][0-9].txt"
    exit 1
fi    

reffile=$1
tmpdir="/mnt/zram/tmp"
outputdir=$2
date=$(echo $(echo "$reffile" | cut -d- -f 2) | cut -d. -f1)
UNIFIED_FILE="UNIFIED"
MERGED_FILE="MERGED"
RESULT_FILE="Hinweissätze-Ergebnisse-${date}.txt"

mkdir -p $tmpdir

#Setup Solr in Ramdisk and import data

#Query all matching IDs from Solr and write files for each term respectively
#cat $reffile | awk --file rewrite_query_expression.awk | xargs -t -0 --max-args=2 --max-procs=8 java -cp $CLASSPATH  $QUERYREF_MAIN $tmpdir
cat $reffile | awk --file rewrite_query_expression.awk | xargs -0 --max-args=2 --max-procs=8 ./query_reference_id.sh $tmpdir

#Remove all files without containing ids
find $tmpdir -size 0 -print0 | xargs -0 rm

#Now derive the reference term from the filename and insert it into the files
cd $tmpdir && find $tmpdir -type f -printf '%P\0' | xargs -0 awk '{TERM = FILENAME; sub(/\..*$/, "", TERM); print $0 "|" TERM > (TERM ".terms")}'

# Create a with with a list of IDs and all matching reference terms
cat $tmpdir/*.terms | sort -k1 > $tmpdir/UNIFIED
awk -F "|" 's != $1 || NR ==1{s=$1;if(p){print p};p=$0;next} {sub($1,"",$0);p=p""$0;}END{print p}' < $tmpdir/$UNIFIED_FILE > $tmpdir/$MERGED_FILE

#Copy file
cp $tmpdir/$MERGED_FILE $outputdir/$RESULT_FILE
#find $tmpdir \( -name "*.ids" -o -name "*.terms" \) -print0 | xargs -0 rm
#rm $tmpdir/$UNIFIED_FILE
