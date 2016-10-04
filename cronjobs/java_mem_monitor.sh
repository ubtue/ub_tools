#!/bin/bash
LOG_FILE=/var/log/ixtheo/java_mem_stats.log
echo "$(date +%F%T) $(jstat -gccapacity $(jps -v | grep -- -Djetty.port=8080 | cut -f1 -d' ') | tail -n 1)" >> "$LOG_FILE" 
