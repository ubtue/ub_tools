#!/bin/bash
set -o errexit

FILE_TO_UPDATE="/usr/local/var/lib/tuelib/cronjobs/initiate_marc_pipeline.conf"

apt-get --quiet --yes install libxxhash-dev

grep -qF "[DiffImport]" "$FILE_TO_UPDATE" || cat << 'EOF' >> "$FILE_TO_UPDATE"

[DiffImport]
title_marc_data     = GesamtTiteldaten-post-pipeline-diff-??????-??????.mrc
authority_marc_data = Normdaten-fully-augmented-diff-??????-??????.mrc
deletion_list       = Deletion-list-??????-??????.mrc
EOF

grep -qF "[FullImportSchedule]" "$FILE_TO_UPDATE" || cat << 'EOF' >> "$FILE_TO_UPDATE"

[FullImportSchedule]
day                 = 7
EOF