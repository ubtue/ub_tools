# Helper functions for pipelines
set -o errexit -o nounset -o pipefail

function ExitHandler {
    (setsid kill -- -$$) &
    exit 1
}
trap ExitHandler SIGINT


function Abort {
    kill -INT $$
}


function StartPhase {
    if [ -z ${PHASE+x} ]; then
        PHASE=1
    else
        ((++PHASE))
    fi
    START=$(date +%s.%N)
    echo -e "*** Phase $PHASE: $1 - $(date) ***" | tee --append "${log}"
}


# Call with "CalculateTimeDifference $start $end".
# $start and $end have to be in seconds.
# Returns the difference in fractional minutes as a string.
function CalculateTimeDifference {
    start=$1
    end=$2
    echo "scale=2;($end - $start)/60" | bc --mathlib
}


function EndPhase {
    PHASE_DURATION=$(CalculateTimeDifference $START $(date +%s.%N))
    echo "Phase ${PHASE}: Done after ${PHASE_DURATION} minutes." | tee --append "${log}"
}


function CleanUp {
    rm -f GesamtTiteldaten-post-phase*.mrc
}


function DetermineDateFromFilename {
   echo $(echo "$1" | cut -d- -f 2) | cut -d. -f1

}
