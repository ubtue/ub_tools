#!/bin/bash
# Send status message depending on exit code  and log on error
# Constraints: log file must be derivable from base_name

function Usage {
    echo "Usage: $0 program_exit_code notification_email_address program_base_name log_dir"
    exit 1

}

if [ $# -ne 4 ]; then
    Usage
fi

program_exit_code="$1"
email="$2"
program_base_name="$3"
log_dir="$4"

priority=$( [ "$program_exit_code" -eq 0 ] && echo "low" || echo "high" )
body=$( [ $program_exit_code -eq 0   ] && echo "--message-body=Success" || \
        echo --message-body-file="$log_dir/$program_base_name.log" )
        send_email --recipients="$email" --priority="$priority" --subject=" $program_base_name (from $(hostname))" "$body"
