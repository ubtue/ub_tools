#!/bin/bash
# Modify crontab to execute the script given as parameter in the minute after the next
set -o errexit -o nounset

function Usage() {
   echo "Usage: $0 script_in_cronjob"
   exit 1
}

if [[ $# != 1 ]]; then
    Usage
fi

scriptname=$1
execution_time=$(date -d '2 minutes' +%H:%M)
hour=$(date --date="${execution_time}" +%H)
minute=$(date --date="${execution_time}" +%M)
crontab -l 2>/dev/null | awk --assign scriptname="${scriptname}" --assign hour="${hour}" --assign minute="${minute}" \
    --field-separator '[ \t]' --file <(cat - <<-'EOF'
    { if (match($0, scriptname) && !match($0, "^#")) {
           $1=""; $2=""
           print minute " " hour $0;
      } else {
        print $0;
      }
    }
EOF
) | crontab -
