#!/bin/bash

set -o errexit

if [ $TUEFIND_FLAVOUR=="ixtheo" ] 
then
  echo "export VUFIND_LOCAL_MODULES=TueFindSearch,TueFind,IxTheo" >> /etc/profile.d/vufind.sh
else
  echo "export VUFIND_LOCAL_MODULES=TueFindSearch,TueFind,KrimDok" >> /etc/profile.d/vufind.sh
fi
