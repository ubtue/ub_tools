#!/bin/bash
set -o errexit -o nounset

readonly CONFIG_PATH=/usr/local/var/lib/tuelib/generate_new_journal_alert_stats.conf
generate_new_journal_alert_stats relbib $(inifile_lookup "$CONFIG_PATH" "" relbib_recipients)
generate_new_journal_alert_stats ixtheo $(inifile_lookup "$CONFIG_PATH" "" ixtheo_recipients)
