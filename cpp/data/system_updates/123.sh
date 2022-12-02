#!/bin/bash

set -o errexit
VUFIND_PROFILE_FILE=/etc/profile.d/vufind.sh

if [ -e "$VUFIND_PROFILE_FILE" ]; then
  if ! $(grep --quiet "VUFIND_LOCAL_MODULES" "$VUFIND_PROFILE_FILE"); then
    case $TUEFIND_FLAVOUR in
    "ixtheo")
      sed --in-place '/VUFIND_LOCAL_DIR/ s/$/\nexport\ VUFIND_LOCAL_MODULES=TueFindSearch,TueFind,IxTheo/' "$VUFIND_PROFILE_FILE"
      ;;
    "krimdok")
      sed --in-place '/VUFIND_LOCAL_DIR/ s/$/\nexport\ VUFIND_LOCAL_MODULES=TueFindSearch,TueFind,KrimDok/' "$VUFIND_PROFILE_FILE"
      ;;
    *)
      exit 0
    esac
  fi
fi