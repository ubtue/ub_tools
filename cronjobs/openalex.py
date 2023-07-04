#!/bin/python3
#
# Purpose of this script is to compare a DOI list from our current MARC data
# against OpenAlex to check how many of them are actually listed.
#
# Goal in this case is to get information about existing citations / references
# and compare the result with OpenCitations.
#
# https://docs.openalex.org/api-entities/works/get-a-single-work

from requests import get
import concurrent.futures
import csv
import json
import sys

if (len(sys.argv) != 3):
    print ("Usage: openalex.py <in_file> <out_file>")
    print ("\t<in_file> is a text file containing 1 DOI per line")
    print ("\t<out_file> will be a csv file containing information about the requests")
    sys.exit(1)

DOI_FILE = open(sys.argv[1], "r")
OUT_FILE = open(sys.argv[2], "w")
WORKER_THREAD_COUNT = 2 # We have a limit of 10 requests per second so the value should be below that
MAX_DOIS_PER_REQUEST = 50 # 50 is max due to documentation, we need to explicitly set this for pagination (else we only get 25 per result)
DOI_PREFIX = "https://doi.org/"

# For rate limiting, see: https://docs.openalex.org/how-to-use-the-api/rate-limits-and-authentication
# To get into "polite pool" we use the mailto: parameter in the query (User-Agent header unfortunately didn't work)
API_ENDPOINT = "https://api.openalex.org/works?mailto=ixtheo-team@ub.uni-tuebingen.de&per-page=" + str(MAX_DOIS_PER_REQUEST) + "&filter=doi:"

# this function can be used for multithreading
# a single API query can request information about up to 50 DOIs
def process_doi_batch(dois):
    rows = []

    query_string = ""
    for doi in dois:
        if query_string:
            query_string += "|"
        query_string += DOI_PREFIX + doi
    query_url = API_ENDPOINT + query_string
    print(query_url)

    try:
        result = get(query_url)
        # print(result.text)
        if result.status_code != 200:
            raise ResultError

        result_json = json.loads(result.text)
        found_dois = []
        for doi_json in result_json["results"]:
            # instead of using the cited_by_count, we count the amount of referenced works
            found_doi = doi_json["doi"].replace(DOI_PREFIX, "")
            row = [found_doi]
            found_dois.append(found_doi)

            try:
                row.append("References: " + str(len(doi_json["referenced_works"])))
            except:
                row.append("JSON Error")
            rows.append(row)

        for doi in dois:
            if not doi in found_dois:
                rows.append([doi, "Not found"])
    except:
        for doi in dois:
            rows.append([doi, "API Error"])

    # print(rows)
    return rows


# init DOI list + write CSV header
dois = DOI_FILE.readlines()
writer = csv.writer(OUT_FILE)
writer.writerow(["DOI", "Result"])
OUT_FILE.flush()

# start multithreaded doi processing
with concurrent.futures.ThreadPoolExecutor(max_workers = WORKER_THREAD_COUNT) as executor:
    print ("Initialize multithreading...")

    # split list into grouped batches
    doi_batches = [dois[i:i + MAX_DOIS_PER_REQUEST] for i in range(0, len(dois), MAX_DOIS_PER_REQUEST)]

    futures = []
    for doi_batch in doi_batches:
        doi_batch_stripped = [doi.rstrip() for doi in doi_batch]
        futures.append(executor.submit(process_doi_batch, doi_batch_stripped))

    print ("Processing results...")
    for future in concurrent.futures.as_completed(futures):
        for row in future.result():
            print (row)
            writer.writerow(row)
