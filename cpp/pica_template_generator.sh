#!/bin/bash
set -o errexit -o nounset

tmpdir=$(mktemp --directory -t pica_generator_XXXXXX)
trap "find ${tmpdir} -mindepth 1 -delete && rmdir ${tmpdir}" EXIT
BIN_BASE_PATH=/usr/local/bin
PROGRAM_BASE_NAME=$(basename ${0%.sh})
CONFIG_BASE_PATH=/usr/local/var/lib/tuelib/${PROGRAM_BASE_NAME}
CONFIG_FILE=${CONFIG_BASE_PATH}/${PROGRAM_BASE_NAME}.conf
TEMPLATE_DIR=/usr/local/var/lib/tuelib/${PROGRAM_BASE_NAME}/pica_generator_templates


function GetIniEntry() {
    local section="$1"
    local entry="$2"
    ${BIN_BASE_PATH}/inifile_lookup ${CONFIG_FILE} ${section} ${entry}
}

function GetFormFile() {
    local mail_content_dir="$1"
    echo $(ls --directory ${mail_content_dir}/*.txt | head -n 1)
}

function GetTextType() {
    local form_file="$1"
    echo $(basename ${form_file%.txt})
}

function GetSystemType() {
    local form_file="$1"
    echo $(head -n 1 ${form_file} | sed -e 's/.*=//')
}

function GetTemplateFiles() {
    local form_type="$1"
    echo ${TEMPLATE_DIR}/${form_type}_a.template ${TEMPLATE_DIR}/${form_type}_o.template
}

function GetOutFiles() {
    local tmpdir="$1"
    local form_type="$2"
    local pica_file_suffix=$(GetIniEntry "FileSpecs" "pica_file_suffix")
    echo ${tmpdir}/${form_type}_a${pica_file_suffix} ${tmpdir}/${form_type}_o${pica_file_suffix}
}

function GetTargetMailAddress() {
    local system_type="$1"
    local form_type="$2"
    local text_type=${form_type%%_*}
    echo $(GetIniEntry "Mail" ${system_type}_${text_type})
}


function GetSubject() {
    local mailfile="$1"
    grep --ignore-case '^subject' ${mailfile} | sed -re 's/^subject:\s+//i' \
        | perl -CS -MEncode -ne 'print decode("MIME-Header", $_)'
}

function GetBodyFile() {
   local tmpdir="$1"
   echo ${tmpdir}/part1
}


function ExpandPicaTemplate() {
   local tmpdir="$1"
   local form_file="$2"
   local form_type="$3"

   # Read in file and split on first '='
   local keys=()
   while read line; do
     IFS="=" read -r key value <<<"$line"
       export "$key"="$value"
       # Needed because we have to explicitly specify variables to be expanded, c.f. envsubst call
       keys+=(\\\$$key)
   done < ${form_file}

   local template_files=($(GetTemplateFiles ${form_type}))
   local out_files=($(GetOutFiles ${tmpdir} ${form_type}))
   envsubst $(IFS=','; echo "${keys[*]}") < ${template_files[0]} > ${out_files[0]}
   envsubst $(IFS=','; echo "${keys[*]}") < ${template_files[1]} > ${out_files[1]}
}

entire_mail_file=${tmpdir}/mail.eml
cat | tee ${entire_mail_file} | munpack -q -t -C ${tmpdir}

form_file=$(GetFormFile ${tmpdir})
form_type=$(GetTextType ${form_file})
system_type=$(GetSystemType ${form_file})

#Skip first line in form_file as the system type does not have to be expanded
ExpandPicaTemplate ${tmpdir} <(tail -n +2 ${form_file}) ${form_type}
out_files=($(GetOutFiles ${tmpdir} ${form_type}))
subject=$(GetSubject ${entire_mail_file})
body_file=$(GetBodyFile ${tmpdir})


cat ${body_file} | mutt \
       -e 'my_hdr From:UB NoReply <noreply@ub.uni-tuebingen.de>' \
       -e 'set charset="utf-8"' \
       -e 'set send_charset="utf-8"' \
       -s "${subject}" \
       -a ${out_files[0]} -a ${out_files[1]} \
       -- $(GetTargetMailAddress ${system_type} ${form_type})
