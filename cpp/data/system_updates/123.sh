#!/bin/bash

set -o errexit
vuscript=/etc/profile.d/vufind.sh

if !(grep --quiet "VUFIND_LOCAL_MODULES" "$vuscript") && [ $TUEFIND_FLAVOUR=="ixtheo" ] 
then
  sed --in-place '/VUFIND_LOCAL_DIR/ s/$/\nexport\ VUFIND_LOCAL_MODULES=TueFindSearch,TueFind,IxTheo/' "$vuscript"
fi

if !(grep --quiet "VUFIND_LOCAL_MODULES" "$vuscript") && [ $TUEFIND_FLAVOUR=="krimdok" ] 
then
  sed --in-place '/VUFIND_LOCAL_DIR/ s/$/\nexport\ VUFIND_LOCAL_MODULES=TueFindSearch,TueFind,KrimDok/' "$vuscript"
fi