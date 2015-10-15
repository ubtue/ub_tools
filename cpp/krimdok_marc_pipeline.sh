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
echo "*** Phase 1 ***"
echo "*** Phase 1 ***" >> "${log}"
krimdok_filter --bibliotheks-sigel-filtern "$2" ÜbergeordneteTitelundLokaldaten-filtered-"${date}".mrc \
               >> "${log}" 2>&1

# Phase 2:
echo "*** Phase 2 ***"
echo "*** Phase 2 ***" >> "${log}"
krimdok_filter --normalise-urls "$1" TitelUndLokaldaten-normalised-"${date}".mrc >> "${log}" 2>&1
krimdok_filter --normalise-urls ÜbergeordneteTitelundLokaldaten-filtered-"${date}".mrc \
               ÜbergeordneteTitelundLokaldaten-filtered-and-normalised-"${date}".mrc >> "${log}" 2>&1
rm -f ÜbergeordneteTitelundLokaldaten-filtered-"${date}".mrc

# Phase 3:
echo "*** Phase 3 ***"
echo "*** Phase 3 ***" >> "${log}"
create_child_refs.sh TitelUndLokaldaten-normalised-"${date}".mrc \
                     ÜbergeordneteTitelundLokaldaten-filtered-and-normalised-"${date}".mrc >> "${log}" 2>&1
add_child_refs ÜbergeordneteTitelundLokaldaten-filtered-and-normalised-"${date}".mrc \
               ÜbergeordneteTitelUndLokaldaten-filtered-and-normalised-with-child-refs-"${date}".mrc \
               child_refs child_titles >> "${log}" 2>&1
add_child_refs TitelUndLokaldaten-normalised-"${date}".mrc \
               TitelUndLokaldaten-normalised-with-child-refs-"${date}".mrc \
               child_refs child_titles >> "${log}" 2>&1
rm -f ÜbergeordneteTitelundLokaldaten-filtered-and-normalised-"${date}".mrc
rm -r TitelUndLokaldaten-normalised-"${date}".mrc
rm -f child_refs child_titles parent_refs

# Phase 4:
echo "*** Phase 4 ***"
echo "*** Phase 4 ***" >> "${log}"
add_isbns_or_issns_to_articles TitelUndLokaldaten-normalised-with-child-refs-"${date}".mrc \
                               ÜbergeordneteTitelUndLokaldaten-filtered-and-normalised-with-child-refs-"${date}".mrc \
                               TitelUndLokaldaten-normalised-with-child-refs-and-issns-"${date}".mrc >> "${log}" 2>&1
rm -f TitelUndLokaldaten-normalised-with-child-refs-"${date}".mrc

# Phase 5:
echo "*** Phase 5 ***"
echo "*** Phase 5 ***" >> "${log}"
create_full_text_db --process-count-low-and-high-watermarks \
                    $(get_config_file_entry.py krimdok_marc_pipeline.conf \
                      create_full_text_db process_count_low_and_high_watermarks) \
                    TitelUndLokaldaten-normalised-with-child-refs-and-issns-"${date}".mrc \
                    TitelUndLokaldaten-normalised-with-child-refs-issns-and-full-text-links-"${date}".mrc \
                    full_text.db >> "${log}" 2>&1
cp full_text.db /var/lib/tuelib/
rm -f TitelUndLokaldaten-normalised-with-child-refs-and-issns-"${date}".mrc
rm -f full_text.db

# Phase 6:
echo "*** Phase 6 ***"
echo "*** Phase 6 ***" >> "${log}"
fix_article_biblio_levels --verbose \
    TitelUndLokaldaten-normalised-with-child-refs-issns-and-full-text-links-"${date}".mrc \
    TitelUndLokaldaten-normalised-with-child-refs-issns-full-text-links-and-fixed-articles-"${date}".mrc
fix_article_biblio_levels --verbose \
    ÜbergeordneteTitelUndLokaldaten-filtered-and-normalised-with-child-refs-"${date}".mrc \
    ÜbergeordneteTitelUndLokaldaten-filtered-normalised-with-child-refs-and-fixed-articles-"${date}".mrc
rm -f TitelUndLokaldaten-normalised-with-child-refs-issns-and-full-text-links-"${date}".mrc
rm -f ÜbergeordneteTitelUndLokaldaten-filtered-and-normalised-with-child-refs-"${date}".mrc

# Phase 7:
echo "*** Phase 7 ***"
echo "*** Phase 7 ***" >> "${log}"
populate_in_tuebingen_available --verbose \
    TitelUndLokaldaten-normalised-with-child-refs-issns-full-text-links-and-fixed-articles-"${date}".mrc \
    TitelUndLokaldaten-normalised-with-child-refs-issns-full-text-links-fixed-articles-and-availability-"${date}".mrc
populate_in_tuebingen_available --verbose \
    ÜbergeordneteTitelUndLokaldaten-filtered-normalised-with-child-refs-and-fixed-articles-"${date}".mrc \
    ÜbergeordneteTitelUndLokaldaten-filtered-normalised-with-child-refs-fixed-articles-and-availability-"${date}".mrc
rm -f TitelUndLokaldaten-normalised-with-child-refs-issns-full-text-links-and-fixed-articles-"${date}".mrc
rm -f ÜbergeordneteTitelUndLokaldaten-filtered-normalised-with-child-refs-and-fixed-articles-"${date}".mrc
