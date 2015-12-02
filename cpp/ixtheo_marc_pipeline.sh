#!/bin/bash
# Runs through the phases of the ixTheo MARC-21 pipeline.
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

log=/tmp/ixtheo_marc_pipeline.log
rm -f "${log}"

# Phase 1:
P=1
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
create_child_refs.sh TitelUndLokaldaten-"${date}".mrc ÜbergeordneteTitelUndLokaldaten-"${date}".mrc >> "${log}" 2>&1
add_child_refs ÜbergeordneteTitelUndLokaldaten-"${date}".mrc \
               ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".mrc \
               child_refs child_titles >> "${log}" 2>&1

# Phase 2:
P=2
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
add_isbns_or_issns_to_articles TitelUndLokaldaten-"${date}".mrc \
                               ÜbergeordneteTitelUndLokaldaten-post-phase$((P-1))-"${date}".mrc \
                               TitelUndLokaldaten-post-phase"$P"-"${date}".mrc >> "${log}" 2>&1

# Phase 3:
P=3
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
enrich_keywords_with_title_words TitelUndLokaldaten-post-phase$((P-1))-"${date}".mrc \
                                 TitelUndLokaldaten-post-phase"$P"-"${date}".mrc \
                                 ../cpp/data/stopwords.???

# Phase 4:
P=4
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
augment_bible_references TitelUndLokaldaten-post-phase$((P-1))-"${date}".mrc \
                         Normdaten-"${date}".mrc \
                         TitelUndLokaldaten-post-phase"$P"-"${date}".mrc \
                         ../cpp/data/BibleOrder.map >> "${log}" 2>&1
cp *.map /var/lib/tuelib/bibleRef/

# Phase 5:
P=5
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
update_ixtheo_notations \
    TitelUndLokaldaten-post-phase$((P-1))-"${date}".mrc \
    TitelUndLokaldaten-post-phase"$P"-"${date}".mrc \
    ../cpp/data/IxTheo_Notation.csv >> "${log}" 2>&1
update_ixtheo_notations \
    ÜbergeordneteTitelUndLokaldaten-post-phase1-"${date}".mrc \
    ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".mrc \
    ../cpp/data/IxTheo_Notation.csv >> "${log}" 2>&1


# Phase 6:
P=6
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
map_ddc_and_rvk_to_ixtheo_notations \
    TitelUndLokaldaten-post-phase$((P-1))-"${date}".mrc \
    TitelUndLokaldaten-post-phase"$P"-"${date}".mrc \
    ../cpp/data/ddc_ixtheo.map ../cpp/data/ddc_ixtheo.map
map_ddc_and_rvk_to_ixtheo_notations \
    ÜbergeordneteTitelUndLokaldaten-post-phase$((P-1))-"${date}".mrc \
    ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".mrc \
    ../cpp/data/ddc_ixtheo.map ../cpp/data/ddc_ixtheo.map


# Phase 7:
P=7
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
fix_article_biblio_levels --verbose \
    TitelUndLokaldaten-post-phase$((P-1))-"${date}".mrc \
    TitelUndLokaldaten-post-pipeline-"${date}".mrc
fix_article_biblio_levels --verbose \
    ÜbergeordneteTitelUndLokaldaten-post-phase$((P-1))-"${date}".mrc \
    ÜbergeordneteTitelUndLokaldaten-post-pipeline-"${date}".mrc

# Cleanup of intermediate files:
for p in $(seq $((P-1))); do
    rm -f ÜbergeordneteTitelUndLokaldaten-post-"$p"-"${date}".mrc
    rm -f TitelUndLokaldaten-post-"$p"-"${date}".mrc
done
rm -f child_refs child_titles parent_refs
