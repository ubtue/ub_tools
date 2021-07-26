#!/usr/bin/python3
import os
import time
import sys
import urllib.request
import xml.etree.ElementTree as ET

BASE_URI='https://kalliope-verbund.info/sru?version=1.2&operation=searchRetrieve&query=ead.origination.gnd='
CHUNK_SIZE=10000
RECORD_SCHEMA='&recordSchema=mods37'
MAXIMUM_RECORDS='&maximumRecords='
START_RECORD='&startRecord='


def GetTotalResults(query):
    with urllib.request.urlopen(BASE_URI + query + MAXIMUM_RECORDS + '0' + RECORD_SCHEMA) as response:
        result = response.read()
        root = ET.fromstring(result)
        num_of_records =  root.find('.//{*}numberOfRecords')
        if num_of_records is None:
            raise RuntimeError('No numberOfRecords')
        return num_of_records.text


def GetChunk(query, offset, fileset):
    uri = BASE_URI + query + RECORD_SCHEMA + MAXIMUM_RECORDS + \
          str(CHUNK_SIZE) + START_RECORD + str(offset)
    print("Download " + uri + "...")
    local_tmp_file, headers = urllib.request.urlretrieve(uri)
    fileset.append(local_tmp_file)


def JoinResultFile(files, outfile):
# Based on https://stackoverflow.com/questions/9004135/merge-multiple-xml-files-from-command-line
    first = None
    for filename in files:
        data = ET.parse(filename).getroot()
        if first is None:
            first = data
        else:
            first.extend(data)
    if first is not None:
         outfile.write(ET.tostring(first))
         

def CleanUp(fileset):
   for tmp_file in fileset:
       print("Removing temporary file : " + tmp_file)
       os.remove(tmp_file)


def Main():
    if len(sys.argv) != 2:
        print("Usage: " + sys.argv[0] + " outfile")
        exit(1)
    outfile = open(sys.argv[1], 'wb') 
    tmp_fileset = []
    for gnd_prefix in range(0,9):
        query = str(gnd_prefix) + '*'
        total_results = GetTotalResults(query)
        print(total_results + ' results altogether for query \"' + query + '\"')
        for chunk_number in range(0, int(int(total_results) / CHUNK_SIZE) + 1):
            GetChunk(query, chunk_number * CHUNK_SIZE + 1, tmp_fileset)
            time.sleep(0.5)
    JoinResultFile(tmp_fileset, outfile)
    CleanUp(tmp_fileset)


try:
    Main()
except Exception as e:
    print("ERROR: " + e)
