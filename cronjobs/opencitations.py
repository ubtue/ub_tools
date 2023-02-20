#!/bin/python3
#
# Purpose of this script is to compare a DOI list from our current MARC data
# against OpenCitations to check how many of them are actually listed.
#
# OpenCitations has a whole Index, as well as several sub-indexes
# like COCI (crossRef), DOCI (DataCite), POCI (PubMed), CROCI (Crowdsourcing)
# and maybe even more. See also:
# http://opencitations.net/querying

from requests import get
import concurrent.futures
import csv
import sys

if (len(sys.argv) != 3):
    print ("Usage: opencitations.py <in_file> <out_file>")
    print ("\t<in_file> is a text file containing 1 DOI per line")
    print ("\t<out_file> will be a csv file containing information about the requests")
    sys.exit(1)

API_KEY_FILE = open("/usr/local/var/lib/tuelib/OpenCitations-API.key", "r")
API_KEY = API_KEY_FILE.read().strip()
DOI_FILE = open(sys.argv[1], "r")
OUT_FILE = open(sys.argv[2], "w")
HTTP_HEADERS = {"authorization": API_KEY}
WORKER_THREAD_COUNT = 5

API_ENDPOINTS = {
    "Indexes": "http://opencitations.net/index/api/v1",
    "COCI": "http://opencitations.net/index/coci/api/v1",
    "DOCI": "http://opencitations.net/index/doci/api/v1",
    "POCI": "http://opencitations.net/index/poci/api/v1",
    "CROCI": "http://opencitations.net/index/croci/api/v1",
}


# this function can be used for multithreading to process a single doi
def process_doi(doi):
    row = [doi]
    for api_endpoint_name in API_ENDPOINTS:
        api_endpoint_url = API_ENDPOINTS[api_endpoint_name] + "/references/" + doi
        result = get(api_endpoint_url, HTTP_HEADERS)
        row.append(result.status_code)

    print(row)
    return row


# init DOI list + write CSV header
dois = DOI_FILE.readlines()
writer = csv.writer(OUT_FILE)
header = ["DOI"]
for api_endpoint_name in API_ENDPOINTS:
    header.append(api_endpoint_name)
writer.writerow(header)

# start multithreaded doi processing
with concurrent.futures.ThreadPoolExecutor(max_workers = WORKER_THREAD_COUNT) as executor:
    print ("Initialize multithreading...")

    futures = []
    for doi in dois:
        doi = doi.strip()
        futures.append(executor.submit(process_doi, doi))

    print ("Processing results...")
    for future in concurrent.futures.as_completed(futures):
        # Note that the output will only be written if the buffer is full.
        # we explicitly do not call flush() for performance reasons.
        writer.writerow(future.result())
