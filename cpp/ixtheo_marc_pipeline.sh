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
echo "*** Phase 1 ***"
echo "*** Phase 1 ***" >> "${log}"
create_child_refs.sh TitelUndLokaldaten-"${date}".mrc ÜbergeordneteTitelUndLokaldaten-"${date}".mrc >> "${log}" 2>&1
add_child_refs ÜbergeordneteTitelUndLokaldaten-"${date}".mrc \
               ÜbergeordneteTitelUndLokaldaten-with-child-refs-"${date}".mrc \
               child_refs child_titles >> "${log}" 2>&1

# Phase 2:
echo "*** Phase 2 ***"
echo "*** Phase 2 ***" >> "${log}"
add_isbns_or_issns_to_articles TitelUndLokaldaten-"${date}".mrc \
                               ÜbergeordneteTitelUndLokaldaten-with-child-refs-"${date}".mrc \
                               TitelUndLokaldaten-with-issns-"${date}".mrc >> "${log}" 2>&1

# Phase 3:
echo "*** Phase 3 ***"
echo "*** Phase 3 ***" >> "${log}"
enrich_keywords_with_title_words TitelUndLokaldaten-with-issns-"${date}".mrc \
                                 TitelUndLokaldaten-with-issns-and-title-keywords-"${date}".mrc \
                                 ../cpp/data/stopwords.???

# Phase 4:
echo "*** Phase 4 ***"
echo "*** Phase 4 ***" >> "${log}"
augment_bible_references TitelUndLokaldaten-with-issns-and-title-keywords-"${date}".mrc \
                         Normdaten-"${date}".mrc \
                         TitelUndLokaldaten-with-issns-title-keywords-and-bible-refs-"${date}".mrc >> "${log}" 2>&1
cp *.map /var/lib/tuelib/bibleRef/

# Phase 5:
echo "*** Phase 5 ***"
echo "*** Phase 5 ***" >> "${log}"
update_ixtheo_notations \
    TitelUndLokaldaten-with-issns-title-keywords-and-bible-refs-"${date}".mrc \
    TitelUndLokaldaten-with-issns-title-keywords-bible-refs-and-ixtheo-notations-"${date}".mrc \
    ../cpp/data/IxTheo_Notation.csv >> "${log}" 2>&1
update_ixtheo_notations \
    ÜbergeordneteTitelUndLokaldaten-with-child-refs-"${date}".mrc \
    ÜbergeordneteTitelUndLokaldaten-with-child-refs-and-ixtheo-notations-"${date}".mrc \
    ../cpp/data/IxTheo_Notation.csv >> "${log}" 2>&1

# Cleanup of intermediate files:
rm -f ÜbergeordneteTitelUndLokaldaten-with-child-refs-"${date}".mrc
rm -f TitelUndLokaldaten-with-issns-"${date}".mrc
rm -f TitelUndLokaldaten-with-issns-and-title-keywords-"${date}".mrc
rm -f child_refs child_titles parent_refs
rm -f TitelUndLokaldaten-with-issns-title-keywords-and-bible-refs-"${date}".mrc

