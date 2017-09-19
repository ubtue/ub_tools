#!/bin/bash

# Downloads records from the BSZ based on PPNs found in the error input file
# and creates a well-formed MARC-XML collection from the downloaded records.

if [[ $# != 3 ]]; then
    echo "usage: $0 error_file biblio_xml_output authority_xml_output"
    exit 1
fi

BIBLIO_OUTPUT_XML="$2"
AUTHORITY_OUTPUT_XML="$3"
DOWNLOADED_XML_FILE=/tmp/downloaded_xml
ERROR_PPNS_FILE=/tmp/error_ppns.list
> "$ERROR_PPNS_FILE"

# Create empty output files:
cat /usr/local/var/lib/tuelib/xml/MARC_XML_HEADER /usr/local/var/lib/tuelib/xml/MARC_XML_TRAILER > "$BIBLIO_OUTPUT_XML"
cat /usr/local/var/lib/tuelib/xml/MARC_XML_HEADER /usr/local/var/lib/tuelib/xml/MARC_XML_TRAILER > "$AUTHORITY_OUTPUT_XML"

declare -i good_count=0
declare -i bad_count=0

for line in $(grep --only-matching ppn:'[^ ]*' "$1"); do
    ppn="${line:4:9}"
    wget --quiet "http://swb.bsz-bw.de/sru/DB=2.1/username=/password=/?query=pica.ppn+%3D+%22${ppn}%22&version=1.1&operation=searchRetrieve&stylesheet=http%3A%2F%2Fswb.bsz-bw.de%2Fsru%2F%3Fxsl%3DsearchRetrieveResponse&recordSchema=marc21&maximumRecords=10&startRecord=1&recordPacking=xml&sortKeys=none&x-info-5-mg-requestGroupings=none" \
        --output-document="$DOWNLOADED_XML_FILE"
    make_marc_xml "$DOWNLOADED_XML_FILE" /tmp/bare_record.xml
    cat /usr/local/var/lib/tuelib/xml/MARC_XML_HEADER /tmp/bare_record.xml /usr/local/var/lib/tuelib/xml/MARC_XML_TRAILER \
        > /tmp/single_record_collection.xml
    if xmllint --noout --schema /usr/local/var/lib/tuelib/xml/MARC21slim.xsd /tmp/single_record_collection.xml; then
        if [[ $(categorise_marc_xml /tmp/single_record_collection.xml) == "BIBLIOGRAPHIC" ]]; then
            append_marc_xml /tmp/single_record_collection.xml "$BIBLIO_OUTPUT_XML"
            ((++good_count))
        elif [[ $(categorise_marc_xml /tmp/single_record_collection.xml) == "AUTHORITY" ]] \
             || [[ $(categorise_marc_xml /tmp/single_record_collection.xml) == "CLASSIFICATION" ]]; then
            append_marc_xml /tmp/single_record_collection.xml "$AUTHORITY_OUTPUT_XML"
            ((++good_count))
        else
            echo "${ppn}" >> "$ERROR_PPNS_FILE"
            ((++bad_count))
        fi
    else
        >&2 echo "Bad XML for PPN ${ppn}!"
        echo "${ppn}" >> "$ERROR_PPNS_FILE"
        ((++bad_count))
    fi
    rm -f "$DOWNLOADED_XML_FILE"
done

echo "Processed $good_count good and $bad_count bad records."
echo "The list of PPNs for which we failed is in ${ERROR_PPNS_FILE}."
