#!/bin/bash

for cifs_mount in $(df --output=target | grep "ZE0"); do
    stat ${cifs_mount} > /dev/null
done

exit 0
