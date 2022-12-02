#!/bin/bash

set -o errexit
vuscript=/etc/profile.d/vufind.sh

if  [ $TUEFIND_FLAVOUR=="ixtheo" ] && !(grep --quiet "VUFIND_LOCAL_MODULES" "$vuscript")
then
  sed --in-place '/VUFIND_LOCAL_DIR/ s/$/\nexport\ VUFIND_LOCAL_MODULES=TueFindSearch,TueFind,IxTheo/' "$vuscript"
fi

if [ $TUEFIND_FLAVOUR=="krimdok" ] && !(grep --quiet "VUFIND_LOCAL_MODULES" "$vuscript")  
then
  sed --in-place '/VUFIND_LOCAL_DIR/ s/$/\nexport\ VUFIND_LOCAL_MODULES=TueFindSearch,TueFind,KrimDok/' "$vuscript"
fi