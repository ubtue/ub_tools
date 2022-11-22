#!/bin/bash
set -o errexit -o nounset -o pipefail

trap RemoveTempFiles EXIT

if [ $# != 2 ]; then
    echo "usage: $0 brill_reference_input_marc.xml output.xml"
    exit 1
fi


function RemoveTempFiles {
    rm ${tmpfile1} ${tmpfile2}
}

TOOL_BASE_PATH="/usr/local/ub_tools/cpp/full_text_conversions/brill/metadata_conversion"
ADJUST_YEAR_XSLT="xsl/adjust_year.xsl"
input_file="$1"
output_file="$2"
tmpfile1=$(mktemp -t marc_clean1XXXXX.xml)
tmpfile2=$(mktemp -t marc_clean2XXXXX.xml)

EECO_superior=$(printf "%s" '773i:Enthalten in\037tBrill Encyclopedia of Early Christianity Online\037dLeiden : Brill, 2018' \
                            '\037g2018' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1042525064\037w(DE-600)2952192-0\037w(DE-576)514794658')

BDRO_superior=$(printf "%s" '773i:Enthalten in\037tThe Brill dictionary of religion\037dLeiden : Brill, 2006' \
                            '\037g2006' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1653634359\037w(DE-576)422642290')

BEHO_superior=$(printf "%s" '773i:Enthalten in\037tBrill'"'"'s Encyclopedia of Hinduism\037dLeiden : Brill, 2012' \
                            '\037g2012' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)734740913\037w(DE-600)2698886-0\037w(DE-576)377732672')

BEJO_superior=$(printf "%s" '773i:Enthalten in\037tBrill'"'"'s Encyclopedia of Jainism Online\037dLeiden : Brill,2020' \
                            '\037g2020' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1693246007')

BERO_superior=$(printf "%s" '773i:Enthalten in\037tBrill'"'"'s Encyclopedia of the religions of the indigenous people of South Asia online' \
                            '\037dLeiden : Brill, 2021' \
                            '\037g2021' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1777997526')

BESO_superior=$(printf "%s" '773i:Enthalten in\037tBrill'"'"'s encyclopedia of Sikhism' \
                            '\037dLeiden : Brill, 2017' \
                            '\037g2017' \
                            '\037w(DE-627)1561270474')

ECO_superior=$(printf "%s" '773i:Enthalten in\037tEncyclopedia of Christianity online' \
                            '\037dLeiden : Brill, 2011' \
                            '\037g2011' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1809053870')

EGPO_superior=$(printf "%s" '773i:Enthalten in\037tBrill'"'"'s encyclopedia of global pentecostalism online' \
                            '\037dLeiden : Brill, 2020' \
                            '\037g2020' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1809055768')

EJIO_superior=$(printf "%s" '773i:Enthalten in\037tEncyclopedia of Jews in the Islamic World' \
                            '\037dLeiden : Brill, 2010' \
                            '\037g2010' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)635135574')

ELRO_superior=$(printf "%s" '773i:Enthalten in\037tEncyclopedia of Law and Religion Online' \
                            '\037dLeiden : Brill, 2015' \
                            '\037g2015' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)840011121')

ENBO_superior=$(printf "%s" '773i:Enthalten in\037tBrill'"'"'s Encyclopedia of Buddhism Online' \
                            '\037dLeiden : Brill, XXXX' \
                            '\037gXXXX' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)XXXXXXX')

LKRO_superior=$(printf "%s" '773i:Enthalten in\037tLexikon für Kirchen- und Religionsrecht' \
                            '\037dLeiden : Brill, 2019' \
                            '\037g2019' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1780808704')

RGG4_superior=$(printf "%s" '773i:Enthalten in\037tReligion in Geschichte und Gegenwart 4 Online' \
                            '\037dXXXXXX' \
                            '\037gXXXXXX' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)XXXXXXXX')

RPPO_superior=$(printf "%s" '773i:Enthalten in\037tReligion Past and Present online' \
                            '\037dLeiden : Brill, 2015' \
                            '\037g2015' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)832783099\037w(DE-600)2829692-8\037w(DE-576)442691327')

VSRO_superior=$(printf "%s" '773i:Enthalten in\037tVocabulary for the Study of Religion Online' \
                            '\037dLeiden : Brill, 2015' \
                            '\037g2015' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)837395917\037w(DE-600)2836071-0\037w(DE-576)445958901')

WCEO_superior=$(printf "%s" '773i:Enthalten in\037tWorld Christian encyclopedia online' \
                            '\037dLeiden : Brill, 2022' \
                            '\037g2022' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1809051452')


# Remove superfluous fields and fix deceased author information
marc_filter ${input_file} ${tmpfile1} \
    --remove-fields '500a:Converted from MODS 3.7 to MARCXML.*' \
    --remove-fields '540a:.*' --remove-fields '787t:' \
    --remove-fields '500a:^...\s*$' \
    --remove-fields '500a:^Author passed away: insert behind name  [(][†✝][)]...$' \
    --remove-fields '008:.*' \
    --replace '100a:700a' '(.*)\s*[(]?†[)]?\s*' '\1' \
    --replace '100a:700a' '^([^,]+)\s+([^,]+)$' '\2, \1'

# Insert additionally needed fields
marc_augmentor ${tmpfile1} ${tmpfile2} \
    --insert-field-if "${EECO_superior}" '001:EECO.*' \
    --insert-field-if "${BDRO_superior}" '001:BDRO.*' \
    --insert-field-if "${BEHO_superior}" '001:BEHO.*' \
    --insert-field-if "${BEJO_superior}" '001:BEJO.*' \
    --insert-field-if "${BERO_superior}" '001:BERO.*' \
    --insert-field-if "${BESO_superior}" '001:BESO.*' \
    --insert-field-if "${ECO_superior}" '001:ECO.*' \
    --insert-field-if "${EGPO_superior}" '001:EGPO.*' \
    --insert-field-if "${EJIO_superior}" '001:EJIO.*' \
    --insert-field-if "${ELRO_superior}" '001:ELRO.*' \
    --insert-field-if "${ENBO_superior}" '001:ENBO.*' \
    --insert-field-if "${LKRO_superior}" '001:LKRO.*' \
    --insert-field-if "${RGG4_superior}" '001:RGG4.*' \
    --insert-field-if "${RPPO_superior}" '001:RPPO.*' \
    --insert-field-if "${VSRO_superior}" '001:VSRO.*' \
    --insert-field-if "${WCEO_superior}" '001:WCEO.*' \
    --insert-field '003:DE-Tue135' \
    --insert-field '005:'$(date +'%Y%m%d%H%M%S')'.0' \
    --insert-field '007:cr|||||' \
    --insert-field '084a:1\0372ssgn' \
    `# tag relbib appropriately` \
    --add-subfield-if '084a:0' '001:BEHO.*|BEJO.*|BERO.*|BESO.*|RPPO.*|VSRO.*' \
    --insert-field '338a:Online-Resource\037bcr\0372rdacarrier' \
    --insert-field '852a:DE-Tue135' \
    --insert-field '935c:uwlx' \
    --insert-field '935a:mteo' \
    --insert-field '936j:XXX' \
    --insert-field 'ELCa:1'

# Fix indicators and year information
cat ${tmpfile2} | \
    xmlstarlet ed -O -u '//_:datafield[@tag="773"]/@ind1' -v "0" \
       -u '//_:datafield[@tag="773"]/@ind2' -v "8" \
       -u '//_:datafield[@tag="936"]/@ind1' -v "u" \
       -u '//_:datafield[@tag="936"]/@ind2' -v "w" |
    xmlstarlet tr ${TOOL_BASE_PATH}/${ADJUST_YEAR_XSLT} \
       > ${output_file}
