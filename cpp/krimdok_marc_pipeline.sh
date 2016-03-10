#!/bin/bash
# Runs through the phases of the KrimDok MARC processing pipeline.
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
logdir=/var/log/krimdok
log="${logdir}/krimdok_marc_pipeline.log"
rm -f "${log}"

P=0
echo "*** Phase $P: Convert MARC-21 to MARC-XML ***"
echo "*** Phase $P: Convert MARC-21 to MARC-XML ***" >> "${log}"
marc_grep TitelUndLokaldaten-"${date}".mrc 'if "001" == ".*" extract *' marc_xml \
    > TitelUndLokaldaten-"${date}".xml 2>> "${log}"
marc_grep ÜbergeordneteTitelUndLokaldaten-"${date}".mrc 'if "001" == ".*" extract *' marc_xml \
    > ÜbergeordneteTitelUndLokaldaten-"${date}".xml 2>> "${log}"
marc_grep Normdaten-"${date}".mrc 'if "001" == ".*" extract *' marc_xml \
    > Normdaten-"${date}".xml 2>> "${log}"

((++P))
echo "*** Phase $P: Filter out Records of Other Institutions ***"
echo "*** Phase $P: Filter out Records of Other Institutions ***" >> "${log}"
krimdok_filter --bibliotheks-sigel-filtern ÜbergeordneteTitelUndLokaldaten-"${date}".xml \
                                           ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".xml \
                                           >> "${log}" 2>&1

((++P))
echo "*** Phase $P: Normalise URL's ***"
echo "*** Phase $P: Normalise URL's ***" >> "${log}"
krimdok_filter --normalise-urls TitelUndLokaldaten-"${date}".xml TitelUndLokaldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1
krimdok_filter --normalise-urls ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
               ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1

((++P))
echo "*** Phase $P: Add ISBN's or ISSN's to Articles ***"
echo "*** Phase $P: Add ISBN's or ISSN's to Articles ***" >> "${log}"
add_isbns_or_issns_to_articles TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                               ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                               TitelUndLokaldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1

((++P))
echo "*** Phase $P: Create Full-Text Database ***"
echo "*** Phase $P: Create Full-Text Database ***" >> "${log}"
create_full_text_db --process-count-low-and-high-watermarks \
                    $(get_config_file_entry.py krimdok_marc_pipeline.conf \
                      create_full_text_db process_count_low_and_high_watermarks) \
                    TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                    TitelUndLokaldaten-post-phase"$P"-"${date}".xml \
                    full_text.db >> "${log}" 2>&1
cp full_text.db /var/lib/tuelib/

((++P))
echo "*** Phase $P: Fill in the \"in_tuebingen_available\" Field ***"
echo "*** Phase $P: Fill in the \"in_tuebingen_available\" Field ***" >> "${log}"
populate_in_tuebingen_available --verbose \
                                TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                                TitelUndLokaldaten-post-phase-"$P"-"${date}".xml >> "${log}" 2>&1
populate_in_tuebingen_available --verbose \
                                ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-3))"-"${date}".xml \
                                ÜbergeordneteTitelUndLokaldaten-post-phase"$P"-"${date}".xml >> "${log}" 2>&1

((++P))
echo "*** Phase $P: Fill in missing 773\$a Subfields ***"
echo "*** Phase $P: Fill in missing 773\$a Subfields ***" >> "${log}"
augment_773a --verbose TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                       ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                       TitelUndLokaldaten-post-pipeline-"${date}".xml >> "${log}" 2>&1
augment_773a --verbose ÜbergeordneteTitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                       TitelUndLokaldaten-post-phase"$((P-1))"-"${date}".xml \
                       ÜbergeordneteTitelUndLokaldaten-post-pipeline-"${date}".xml >> "${log}" 2>&1

echo "*** Cleanup of Intermediate Files ***"
echo "*** Cleanup of Intermediate Files ***" >> "${log}"
for p in $(seq "$((P-1))"); do
    rm -f ÜbergeordneteTitelUndLokaldaten-post-"$p"-"${date}".xml
    rm -f TitelUndLokaldaten-post-"$p"-"${date}".xml
done
rm -f full_text.db

echo "*** KRIMDOK MARC PIPELINE DONE ***"
echo "*** KRIMDOK MARC PIPELINE DONE ***" >> "${log}"
