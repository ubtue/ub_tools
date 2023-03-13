#!/bin/bash
set -o errexit -o nounset -o pipefail

HTML_DIR="html"
TEXT_DIR="txt"
XSLT_FILE="/tmp/Encyclopedia.xsl"


function ExtractMetadataInformation {
    # Generate header analogous to e.g.
    local dir=$(dirname $1)
    local xml_file=$(basename $1)
    echo $(xmlstarlet sel -t -v '//mainentry' "${xml_file}")
    echo $(xmlstarlet sel -t -v '//contributorgroup/name/@normal' "${xml_file}" | tr '\n' ';')
    echo
    echo $(xmlstarlet sel -t -v '//*[self::complexarticle or self::simplearticle]/@idno.doi' "${xml_file}" | \
           sed --regexp-extended --expression 's#https?://dx.doi.org/##')
    echo
    echo
    echo
    echo "${dir}/${HTML_DIR}/${file%.xml}.html"
}


function ConvertSubdir {
    local dir="$1"
    cd "${dir}"
    local files_to_convert=$(ls -1 *.xml)
    mkdir -p ${HTML_DIR}
    mkdir -p ${TEXT_DIR}
    for file in ${files_to_convert}; do
        html_file="${HTML_DIR}/${file%.xml}.html"
        text_file="${TEXT_DIR}/${file%.xml}.txt"
        echo "${file} => ${html_file}"
        xmlstarlet tr ${XSLT_FILE} ${file} > ${html_file}
        tidy --quiet true --show-warnings false -modify -wrap 0 -i ${html_file} || if [[ $? == 2 ]]; then echo "Error in tidy"; exit 1; fi
        echo "${html_file} => ${text_file}"
        cat <(ExtractMetadataInformation $(pwd)/${file}) > "${text_file}"
        html2text -utf8 -style pretty "${html_file}" >> "${text_file}"
    done
    cd -
}

function MoveToOutDir {
    local dir="$1"
    local outdir="$2"
    mv "${dir}/fulltextxml/${TEXT_DIR}" "${outdir}"
    mv "${dir}/fulltextxml/${HTML_DIR}" "${outdir}"
}


function AdjustFulltextFileReferences {
    local outdir="$1"
    local path_prefix="$2"
    find "${outdir}" -maxdepth 1 -iname '*.txt' \
        | xargs -n 1 -I'{}' sed -i -re 's#.*/fulltextxml/('"${HTML_DIR}"'.*)#'"${path_prefix}"'/\1#' '{}'
}


function UnpackAndConvertArchives {
    local dir="$1"
    local outdir="$2"
    local path_prefix="$3"
    cd ${dir}
    for archive in $(find . -iname '*.zip'); do
        dir=${archive%.*}
        if [[ -d "${dir}" ]]; then
            echo "${dir} already exists - skipping..."
            continue
        fi
        unzip -o ${archive}
        echo "Converting ${dir}/fulltextxml"
        ConvertSubdir "${dir}/fulltextxml"
        MoveToOutDir "${dir}" "${outdir}/${archive%.*}"
        AdjustFulltextFileReferences "${outdir}/${archive%.*}" "${path_prefix}/${archive%.*}/"
    done
    cd -
}


if [ $# != 3 ]; then
    echo "Usage $0 brill_refterm_archives_root_dir outdir path_prefix"
    exit 1
fi

UnpackAndConvertArchives "$1" "$2" "$3"
