#!/bin/bash

bsz_od_url="https://swblod.bsz-bw.de/od/"

for archive in $(curl ${bsz_od_url} | grep -Po 'href="\Kod-full[^"]+(?=")')
do 
    time  wget ${bsz_od_url}/${archive}
done
