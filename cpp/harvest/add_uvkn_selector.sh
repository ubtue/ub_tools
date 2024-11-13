# Script to add the uvkn-Abrufzeichen (=unverknüpft/unlinked selector)
# for records where there is no 773$w (i.e. linking to a superior work
#!/bin/bash


if [ $# != 2 ]; then
    echo "Usage: $0 infile outfile"
    exit 1
fi

infile="$1"
outfile="$2"

no_superior_ppns=$(marc_grep --input-format=marc-21 <(marc_grep ${infile} 'if "773" exists extract *' marc_binary) 'if "773w" is_missing extract "001"' no_label)
arguments="marc_augmentor ${infile} ${outfile}" 
for no_superior_ppn in ${no_superior_ppns}; do
    arguments+=" --insert-field-if  '935a:uvkn\\x1F2LOK' '001:^${no_superior_ppn}$'"
done
eval "${arguments}"


