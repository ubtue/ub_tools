#!/usr/bin/python3
import re
import sys
import xml.etree.ElementTree as ET

valid_gnd = re.compile('[0-9\-X]+')

def Main():
    if len(sys.argv) != 2:
        print("Usage: " + sys.argv[0] + " kalliope_originator_record_file")
        exit(1)
    root = ET.parse(sys.argv[1]).getroot()
    gnds_and_type = {}
    for recordData in root.findall('.//{*}recordData'):
        genre = recordData.find('.//{*}genre')
        name = recordData.find('.//{*}name')
        if genre is not None and name is not None :
            gnd = name.get('valueURI').replace('https://d-nb.info/gnd/','') if name.get('valueURI') else None
            originator_type = genre.text
            if gnd and originator_type and valid_gnd.match(gnd):
                if gnd in gnds_and_type and not originator_type in gnds_and_type[gnd]:
                    gnds_and_type[gnd].add(originator_type)
                else:
                    gnds_and_type[gnd] =  { originator_type }
    for gnd, originator_type in gnds_and_type.items():
        print(gnd, ' - ', end='')
        print(*originator_type, sep=', ')


try:
   Main()
except Exception as e:
    print("ERROR: " + e)


