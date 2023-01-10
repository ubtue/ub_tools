#!/bin/python3
#
#    @brief  The automation script to organise process of checking the new release of gnd file
#               if there is a new release then download the file, extract it, parse using jq, and
#               generate a new gnd_wiki file using add_autorithy_external_ref.
#           The output file format is csv.
#
#    @author Steven Lolong (steven.lolong@uni-tuebingen.de)
#
#    Copyright (C) 2022 Library of the University of Tübingen
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU Affero General Public License as
#    published by the Free Software Foundation, either version 3 of the
#    License, or (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Affero General Public License for more details.
#
#    You should have received a copy of the GNU Affero General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
Process logic:
1. Check whether the gnd file "authorities-gnd-person_lds_*.jsonld.gz" on https://data.dnb.de/opendata/ is newer
    than the last successful parse to gnd_wiki.csv. 
    To doing this, there is a config file "/mnt/ZE020150/FID-Entwicklung/ub_tools/config_file_add_authority_ext_ref.cnf" that contain 2 lines of information the first line for the lastest successful date generating gnd_wiki data and the second line is the information about the lastest version on the web.
    If the date of successful generating gnd_wiki (on first line) is older compare with the one on the web then:
    a. Download the newer file from the web and put it into folder "/tmp"
    b. The download file is a zip file, it needs to be extracted first
    c. Get the information needed by gnd_wiki and put it into the file "/tmp/input_file_for_add_authority_external_ref.txt" using jq:
        jq -c --stream '.' < authorities-gnd-person_lds.jsonld |grep -E 'https\:/\/d-nb\.info\/gnd\/|wikidata|wikipedia' > input_file_for_add_authority_external_ref.txt
    d. Run "add_authority_external_ref" program. This program will check whether the lastest version date is newer compare to the lastest successful date generating gnd_wiki, if so then generate a new gnd_wiki.csv file. If it success generating gnd_wiki file then update the date on the config file.
    e. Update the information in the config file

"""

import os
import sys
import re
import requests
import util
from urllib.request import urlopen
import traceback
from tqdm import tqdm
import functools
import shutil

# this script needs:
# 1. requests
# 2. tqdm

# Start global variable

base_url = "https://data.dnb.de/opendata/"
config_file = "/mnt/ZE020150/FID-Entwicklung/ub_tools/config_file_add_authority_ext_ref.cnf"
share_folder = "/mnt/ZE020150/FID-Entwicklung/ub_tools/"
# config_file = "/tmp/config_file_add_authority_ext_ref.cnf"
# share_folder = "/tmp/"
gnd_wiki_file = "gnd_to_wiki.csv"
current_file_date_int = 0
input_file_name_for_add_auth = "input_file_for_add_authority_external_ref.txt"


# End global variable


def DownloadTheFile(file_name):
    global base_url
    url_path = f'{base_url}{file_name}'
    target_file = f'/tmp/{file_name}'
    dw = requests.get(url_path, stream=True)
    if dw.status_code != 200:
        dw.raise_for_status()  # Will only raise for 4xx codes, so...
        raise RuntimeError(
            f"Request to {url_path} returned status code {dw.status_code}")

    file_size = int(dw.headers.get('Content-Length', 0))

    desc = "(Unknown total file size)" if file_size == 0 else ""
    dw.raw.read = functools.partial(
        dw.raw.read, decode_content=True)  # Decompress if needed
    with tqdm.wrapattr(dw.raw, "read", total=file_size, desc=desc) as r_raw:
        f = open(target_file, "wb")
        shutil.copyfileobj(r_raw, f)
    return True


def UpdateConfigFile(config_file, successful_generate_date):
    add_auth_ext_ref_config_file = open(config_file, "w+")
    add_auth_ext_ref_config_file.writelines(
        [str(successful_generate_date), "\n"])


def IsThereANewlyReleasedFile():
    file_name = ""
    last_file_update_date_int = 0
    global config_file
    global current_file_date_int
    global base_url

    base_name = "authorities-gnd-person_lds_"
    webpage_object = urlopen(base_url)
    html_bytes = webpage_object.read()
    html = html_bytes.decode("utf-8")

    if re.search('authorities-gnd-person_lds_.*\.jsonld\.gz', html):
        file_name = re.search(
            'authorities-gnd-person_lds_.*\.jsonld\.gz', html).group(0)
        current_file_date_int = int(
            file_name[len(base_name):len(base_name) + 8])
        if (os.path.exists(config_file)):
            add_auth_ext_ref_config_file = open(config_file, "r")
            read_line_1 = add_auth_ext_ref_config_file.readline()
            add_auth_ext_ref_config_file.close

            if (read_line_1.strip()):
                last_file_update_date_int = int(read_line_1)

            if (last_file_update_date_int < current_file_date_int):
                return True

        else:
            # The config file is not exist, assume it needs to create gnd_wiki file
            UpdateConfigFile(config_file, "0")
            return True

    return False


def Main():
    if len(sys.argv) != 2:
        util.SendEmail(os.path.basename(sys.argv[0]),
                       "This script needs to be called with an email address and the system type!\n", priority=1)
        sys.exit(-1)
    util.default_email_recipient = sys.argv[1]

    # 1. Check whether the date of file on the web is newer compare to the last date successful update
    print("Process 1/7 -- Check whether the file on the web is newer")
    if IsThereANewlyReleasedFile():
        # a. Download the newer file
        newer_gz_file_name = "authorities-gnd-person_lds_" + \
            str(current_file_date_int) + ".jsonld.gz"
        newer_file_name = "authorities-gnd-person_lds_" + \
            str(current_file_date_int) + ".jsonld"
        print("Process 2/7 -- Downloading file")
        if DownloadTheFile(newer_gz_file_name):
            # b. The download file is a zip file, it needs to be extracted first
            print("Process 3/7 -- Extracting the new file (.gz)")
            util.ExecOrDie(util.Which("gunzip"), [
                           "-f", f"/tmp/{newer_gz_file_name}"])
            #  c. Get the information needed by gnd_wiki and put it into the file
            print(
                "Process 4/7 -- Parse the file and extract the essential information needed")
            jq_prog_with_pipe = f"jq -c --stream '.' < /tmp/{newer_file_name} | grep -E 'https\:/\/d-nb\.info\/gnd\/|wikidata|wikipedia' > /tmp/{input_file_name_for_add_auth}"
            if os.system(jq_prog_with_pipe) == 0:
                # d. Run "add_authority_external_ref" program
                print("Process 5/7 -- Generate a new gnd_wiki_file")
                util.ExecOrDie(util.Which("add_authority_external_ref"), [
                               "--create_mapping_file", f"/tmp/{input_file_name_for_add_auth}", f"/tmp/{gnd_wiki_file}"])

                print("Process 6/7 -- Copy the gnd_wiki_file to its destination")
                util.ExecOrDie(util.Which("cp"), [
                               "-f", f"/tmp/{gnd_wiki_file}", f"{share_folder}{gnd_wiki_file}"])

                #  e. Update the latest version date on the config file (the second line)
                print(
                    "Process 7/7 -- Updating the config file and remove temporary files")
                UpdateConfigFile(
                    config_file, current_file_date_int)
                util.ExecOrDie(util.Which("rm"), [
                               "-f", f"/tmp/{newer_file_name}"])
                util.ExecOrDie(util.Which("rm"), [
                               "-f", f"/tmp/{input_file_name_for_add_auth}"])
                util.ExecOrDie(util.Which("rm"), [
                               "-f", f"/tmp/{gnd_wiki_file}"])

                util.SendEmail("gnd_wiki_file generator",
                               "Successfully generated gnd_wiki_file.", priority=5)

            else:
                util.SendEmail("gnd_wiki_file generator",
                               "Error while running: " + jq_prog_with_pipe, priority=1)
        else:
            util.SendEmail("gnd_wiki_file generator",
                           "Successfully generated gnd_wiki_file.", priority=1)
    else:
        util.SendEmail("gnd_wiki_file generator",
                       "The file version on the web is the same or older.", priority=5)


try:
    Main()
except Exception as e:
    util.SendEmail("gnd_wiki_file generator", "An unexpected error occurred: "
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
