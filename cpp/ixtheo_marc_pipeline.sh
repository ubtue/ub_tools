#!/bin/bash
# Runs through the phases of the ixTheo MARC processing pipeline.
set -o errexit -o nounset

if [ $# != 3 ]; then
    echo "usage: $0 TitelUndLokaldaten-DDMMYY.mrc ÜbergeordneteTitelUndLokaldaten-DDMMYY.mrc" \
         "Normdaten-DDMMYY.mrc"
    exit 1
fi

if [[ ! "$1" =~ TitelUndLokaldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc ]]; then
    echo 'Die Titeldatendatei entspicht nicht dem Muster TitelUndLokaldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc!'
    exit 1
fi

# Extract date:
date=$(echo $(echo "$1" | cut -d- -f 2) | cut -d. -f1)

if [[ ! "$2" =~ ÜbergeordneteTitelUndLokaldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc ]]; then
    echo 'Die Übergeordnetedatendatei entspicht nicht dem Muster ÜbergeordneteTitelUndLokaldaten-[0-9][0-9][0-9][0-9][0-9][0-9].mrc!'
    exit 1
fi

date2=$(echo $(echo "$2" | cut -d- -f 2) | cut -d. -f1)
if [[ "$date" != "$date2" ]]; then
    echo "Datum im Titeldatendateinamen muss identisch zum Datum im Dateinamen der übergeordneten Daten."
    exit 1
fi


# Set up the log file:
logdir=/var/log/ixtheo
log="${logdir}/ixtheo_marc_pipeline.log"
rm -f "${log}"

P=0
echo "*** Phase $P: Translate MARC-21 to MARC_XML - $(date) ***"
echo "*** Phase $P: Translate MARC-21 to MARC_XML - $(date) ***" >> "${log}"
marc_grep TitelUndLokaldaten-"${date}".mrc 'if "001" == ".*" extract *' marc_xml \
    > TitelUndLokaldaten-"${date}".xml 2>> "${log}"
marc_grep ÜbergeordneteTitelUndLokaldaten-"${date}".mrc 'if "001" == ".*" extract *' marc_xml \
    > ÜbergeordneteTitelUndLokaldaten-"${date}".xml 2>> "${log}"
marc_grep Normdaten-"${date}".mrc 'if "001" == ".*" extract *' marc_xml \
    > Normdaten-"${date}".xml 2>> "${log}"


((++P))
echo "*** Phase $P: Create Author Synonym Map - $(date) ***"
echo "*** Phase $P: Create Author Synonym Map - $(date) ***" >> "${log}"
mkdir --parents /usr/local/vufind2/import/synonym_maps
create_author_synonym_map Normdaten-"${date}".xml /usr/local/vufind2/import/synonym_maps/author_synonyms.map

((++P))
echo "*** Phase $P: Extract Translation Keywords - $(date) ***"
echo "*** Phase $P: Extract Translation Keywords - $(date) ***" >> "${log}"
extract_keywords_for_translation TitelUndLokaldaten-"${date}".xml ÜbergeordneteTitelUndLokaldaten-"${date}".xml \
                                 Normdaten-"${date}".xml >> "${log}" 2>&1

((++P))
echo "*** Phase $P: Parent-to-Child Linking - $(date) ***"
echo "*** Phase $P: Parent-to-Child Linking - $(date) ***" >> "${log}"
create_child_refs.sh TitelUndLokaldaten-"${date}".xml ÜbergeordneteTitelUndLokaldaten-"${date}".xml >> "${log}" 2>&1
add_child_refs ÜbergeordneteTitelUndLokaldaten-"${date}".xml \
               ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".xml \
               child_refs child_titles >> "${log}" 2>&1

((++P))
echo "*** Phase $P: Adding of ISBN'S and ISSN's to Component Parts - $(date) ***"
echo "*** Phase $P: Adding of ISBN'S and ISSN's to Component Parts - $(date) ***" >> "${log}"
add_isbns_or_issns_to_articles TitelUndLokaldaten-"${date}".xml \
                               ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                               TitelUndLokaldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1

((++P))
echo "*** Phase $P: Extracting Keywords from Titles - $(date) ***"
echo "*** Phase $P: Extracting Keywords from Titles - $(date) ***" >> "${log}"
enrich_keywords_with_title_words TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                                 TitelUndLokaldaten-post-phase"$P"-"${date}".xml \
                                 ../cpp/data/stopwords.???

((++P))
echo "*** Phase $P: Augment Bible References - $(date) ***"
echo "*** Phase $P: Augment Bible References - $(date) ***" >> "${log}"
augment_bible_references TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                         Normdaten-"${date}".xml \
                         TitelUndLokaldaten-post-phase"$P"-"${date}".xml \
                         ../cpp/data/BibleOrder.map >> "${log}" 2>&1
cp *.map /var/lib/tuelib/bibleRef/

((++P))
echo "*** Phase $P: Update IxTheo Notations - $(date) ***"
echo "*** Phase $P: Update IxTheo Notations - $(date) ***" >> "${log}"
update_ixtheo_notations \
    TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
    TitelUndLokaldaten-post-phase"$P"-"${date}".xml \
    ../cpp/data/IxTheo_Notation.csv >> "${log}" 2>&1
update_ixtheo_notations \
    ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-4))"-"${date}".xml \
    ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".xml \
    ../cpp/data/IxTheo_Notation.csv >> "${log}" 2>&1


((++P))
echo "*** Phase $P: Map DDC and RVK to IxTheo Notations - $(date) ***"
echo "*** Phase $P: Map DDC and RVK to IxTheo Notations - $(date) ***" >> "${log}"
map_ddc_and_rvk_to_ixtheo_notations \
    TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
    TitelUndLokaldaten-post-pipeline-"${date}".xml \
    ../cpp/data/ddc_ixtheo.map ../cpp/data/ddc_ixtheo.map >> "${log}" 2>&1
map_ddc_and_rvk_to_ixtheo_notations \
    ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
    ÜbergeordneteTitelUndLokaldaten-post-pipeline-"${date}".xml \
    ../cpp/data/ddc_ixtheo.map ../cpp/data/ddc_ixtheo.map >> "${log}" 2>&1


echo "*** Cleanup of Intermediate Files - $(date) ***"
echo "*** Cleanup of Intermediate Files - $(date) ***" >> "${log}"
for p in $(seq "$((P-1))"); do
    rm -f ÜbergeordneteTitelUndLokaldaten-post-"$p"-"${date}".xml
    rm -f TitelUndLokaldaten-post-"$p"-"${date}".xml
done
rm -f child_refs child_titles parent_refs

echo "*** IXTHEO MARC PIPELINE DONE - $(date) ***"
echo "*** IXTHEO MARC PIPELINE DONE - $(date) ***" >> "${log}"
