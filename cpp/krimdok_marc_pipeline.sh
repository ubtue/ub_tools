#!/bin/bash
# Runs through the phases of the KrimDok MARC-21 pipeline.
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

log=/tmp/krimdok_marc_pipeline.log
rm -f "${log}"

# Phase 1:
P=1
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
krimdok_filter --bibliotheks-sigel-filtern "$2" ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".mrc \
               >> "${log}" 2>&1

# Phase 2:
P=2
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
krimdok_filter --normalise-urls "$1" TitelUndLokaldaten-post-phase"$P"-"${date}".mrc >> "${log}" 2>&1
krimdok_filter --normalise-urls ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-1))"-"${date}".mrc \
               ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".mrc >> "${log}" 2>&1

# Phase 3:
P=3
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
add_isbns_or_issns_to_articles TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".mrc \
                               ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-1))"-"${date}".mrc \
                               TitelUndLokaldaten-post-phase"$P"-"${date}".mrc >> "${log}" 2>&1

# Phase 4:
P=4
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
create_full_text_db --process-count-low-and-high-watermarks \
                    $(get_config_file_entry.py krimdok_marc_pipeline.conf \
                      create_full_text_db process_count_low_and_high_watermarks) \
                    TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".mrc \
                    TitelUndLokaldaten-post-phase"$P"-"${date}".mrc \
                    full_text.db >> "${log}" 2>&1
cp full_text.db /var/lib/tuelib/

# Phase 5:
P=5
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
fix_article_biblio_levels --verbose \
                          TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".mrc \
                          TitelUndLokaldaten-post-phase"$P"-"${date}".mrc  >> "${log}" 2>&1
fix_article_biblio_levels --verbose \
                          ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-3))"-"${date}".mrc \
                          ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".mrc  >> "${log}" 2>&1

# Phase 6:
P=6
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
populate_in_tuebingen_available --verbose \
                                TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".mrc \
                                TitelUndLokaldaten-post-phase"$P"-"${date}".mrc >> "${log}" 2>&1
populate_in_tuebingen_available --verbose \
                                ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-1))"-"${date}".mrc \
                                ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".mrc >> "${log}" 2>&1

# Phase 7:
P=7
echo "*** Phase $P ***"
echo "*** Phase $P ***" >> "${log}"
fix_article_biblio_levels --verbose \
    TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".mrc \
    ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-2))"-"${date}".mrc \
    TitelUndLokaldaten-post-pipeline-"${date}".mrc >> "${log}" 2>&1
fix_article_biblio_levels --verbose \
    ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-2))"-"${date}".mrc \
    ÜbergeordneteTitelUndLokaldaten-post-pipeline-"${date}".mrc >> "${log}" 2>&1

# Cleanup of intermediate files:
for p in $(seq "$((P-1))"); do
    rm -f ÜbergeordneteTitelUndLokaldaten-post-"$p"-"${date}".mrc
    rm -f TitelUndLokaldaten-post-"$p"-"${date}".mrc
done
rm -f full_text.db

echo "*** KRIMDOK MARC PIPELINE DONE ***"
echo "*** KRIMDOK MARC PIPELINE DONE ***" >> "${log}"
