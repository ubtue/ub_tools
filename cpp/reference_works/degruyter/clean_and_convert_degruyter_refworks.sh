#!/bin/bash
set -o nounset -o pipefail -o errexit

function Usage {
    echo "Usage $0 journal_prefix marc_in marc_xml_out"
    exit 1
}


trap CleanUpTmpFiles EXIT

NUM_OF_TMPFILES=5
function GenerateTmpFiles {
    for i in $(seq 1 ${NUM_OF_TMPFILES}); do
        tmpfiles+=($(mktemp --tmpdir $(basename -- ${marc_out%.*}).XXXX.${marc_out##*.}));
    done
}


function CleanUpTmpFiles {
   for tmpfile in ${tmpfiles[@]}; do rm ${tmpfile}; done
}


function NormalizePPN {
    journal_prefix="$1"
    infile="$2"
    tmp_outfile="$3"
    # Save original PPN in KEY-field
    marc_augmentor ${infile} ${tmp_outfile} \
        --insert-field-if-regex 'KEYa:/(.+)/\1/g' '001:(.+)'

    # Generate counting PPN with journal prefix
    cat ${tmp_outfile} | \
        sed -r -e 's#(<controlfield tag="001">)[^<]+(</controlfield>)#\1DUMMY_PPN\2#' | \
        awk 'BEGIN {x=1} { if (/controlfield tag="001"/) sub("DUMMY_PPN", sprintf("'${journal_prefix}'%07d", x++)); print $0;}'
}


EBR_superior=$(printf "%s" '773i:Enthalten in\037tEncyclopedia of the Bible and its reception' \
                            '\037dBerlin : De Gruyter, 2002' \
                            '\037g2002' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)612654680')

BHM_superior=$(printf "%s" '773i:Enthalten in\037tLexikon der Bibelhermeneutik' \
                            '\037dBerlin : De Gruyter, 2012' \
                            '\037g2012' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1654493813')

TRE_superior=$(printf "%s" '773i:Enthalten in\037tTheologische Realenzyklopädie' \
                            '\037dBerlin : De Gruyter, 2008' \
                            '\037g2008' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1648438466')


EJCRO_superior=$(printf "%s" '773i:Enthalten in\037tEncyclopedia of Jewish-Christian relations online' \
                            '\037dBerlin : De Gruyter, 2019' \
                            '\037g2019' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1015234194')

URBREL_superior=$(printf "%s" '773i:Enthalten in\037tReligion and urbanity online' \
                            '\037dBerlin : De Gruyter, 2020' \
                            '\037g2020' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1735471887')

RMO_superior=$(printf "%s" '773i:Enthalten in\037tReligious Minorities Online' \
                            '\037dBerlin : De Gruyter, 2023' \
                            '\037g2023' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1795825812')

TILLO_superior=$(printf "%s" '773i:Enthalten in\037tTillich online' \
                            '\037dBerlin : De Gruyter, XXXX' \
                            '\037g2021' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)1684970830')

IBW_superior=$(printf "%s" '773i:Enthalten in\037tImages of the Biblical world online' \
                            '\037dBerlin : De Gruyter, 2014' \
                            '\037g2014' \
                            '\037hOnline-Ressource' \
                            '\037w(DE-627)783963122')

if [ $# != 3 ]; then
    Usage
fi

journal_prefix="$1"
marc_in="$2"
marc_out="$3"

tmpfiles=()
GenerateTmpFiles
#    marc_filter ${marc_in} ${marc_out} --drop '001:EBR' \
#    marc_filter ${marc_in} ${tmpfiles[0]} --drop '001:'${journal_prefix} \
    marc_filter ${marc_in} ${tmpfiles[0]} \
        --remove-fields '856u:^https://www.degruyter.com/document/cover/dbid/ebr/product_pages$' \
        --remove-fields '856u:^https://doi.org/10.1515/ebr$' \
        --remove-fields '003:.*' \
        --remove-fields '005:.*' \
        --remove-fields '006:.*' \
        --remove-fields '007:.*' \
        --remove-fields '008:.*' \
        --remove-fields '035:.*' \
        --remove-fields '040:.*' \
        --remove-fields '044:.*' \
        --remove-fields '072:.*' \
        --remove-fields '300:.*' \
        --remove-fields '336:.*' \
        --remove-fields '337:.*' \
        --remove-fields '338:.*' \
        --remove-fields '347:.*' \
        --remove-fields '490:.*' \
        --remove-fields '506:.*' \
        --remove-fields '530:.*' \
        --remove-fields '538:.*' \
        --remove-fields '546:.*' \
        --remove-fields '588:.*' \
        --remove-fields '650:.*' \
        --remove-fields '655:.*' \
        --remove-fields '773:.*' \
        --remove-fields '776:.*' \
        --remove-fields '912:.*' \
        --remove-subfields '100e:author\.' \
        --remove-subfields '700e:author\.' \

        NormalizePPN ${journal_prefix} ${tmpfiles[0]} ${tmpfiles[1]} > ${tmpfiles[2]}

    # Further cleaning - remove superfluous LOC-reference and fix some garbage from the original files
    cat ${tmpfiles[2]} | xmlstarlet ed -d "//_:datafield[@tag='100' or @tag='700']/_:subfield[@code='4' and starts-with(text(), 'http')]" | \
        saxonb-xslt -xsl:xsl/clean_fields_2.0.xsl - \
        > ${tmpfiles[3]}

    # Augment with our necessary fields
    marc_augmentor ${tmpfiles[3]} ${tmpfiles[4]} \
        --insert-field '003:DE-Tue135' \
        --insert-field '005:'$(date +'%Y%m%d%H%M%S')'.0' \
        --insert-field '007:cr|||||' \
        --insert-field-if '084a:1\0372ssgn' '001:^(?!(EJCRO|URBREL|RMO).*)' \
        --insert-field-if '084a:0\037a1\0372ssgn' '001:^(EJCRO|URBREL|RMO).*' \
        --insert-field '852a:DE-Tue135' \
        --insert-field '935c:uwlx' \
        --insert-field '935a:mteo' \
        --insert-field '936j:XXX' \
        --insert-field '935a:lxgr\0372LOK' \
        --insert-field 'ELCa:1' \
        --insert-field '912a:NOMM' \
        --insert-field-if "${EBR_superior}" '001:EBR.*' \
        --insert-field-if "${BHM_superior}" '001:BHM.*' \
        --insert-field-if "${TRE_superior}" '001:TRE.*' \
        --insert-field-if "${EJCRO_superior}" '001:EJCRO.*' \
        --insert-field-if "${URBREL_superior}" '001:URBREL.*' \
        --insert-field-if "${TILLO_superior}" '001:TILLO.*' \
        --insert-field-if "${IBW_superior}" '001:IBW.*' \
        --insert-field-if "${RMO_superior}" '001:RMO.*' \
        --insert-field-if "935a:BIIN" '001:^(EBR|BHM|TRE|IBW).*' \
        --insert-field-if '1000:(DE-588)121069494\037aSchwank, Benedikt\0374aut\037eVerfasserIn' '001:IBW.*' \
        --replace-subfield-if-regex '245a:/\s+\/\s+/\//g' '245a:\s+/\s+' \
        --add-subfield-if '041a:ger' '001:IBW.*' \
        --replace-subfield-if-regex 'VOLa:/^0(\d)/\1/' 'VOLa:^0.*'

    cat ${tmpfiles[4]} | xmlstarlet tr xsl/adjust_year.xsl  \
        |  xmlstarlet tr xsl/fix_leader.xsl \
        | saxonb-xslt -xsl:xsl/fix_indicators.xsl - \
        | saxonb-xslt  -xsl:xsl/add_volume.xsl  - \
        > ${marc_out}

echo "Finished conversion"



