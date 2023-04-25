#!/bin/python3
#
#    @brief  The automation script to organise process of checking whether the issn is in k10plus if so then get the data and put it in the output file.
#           The output file format is mrc.
#
#    @author Steven Lolong (steven.lolong@uni-tuebingen.de)
#
#    Copyright (C) 2023 Library of the University of Tübingen
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

import re
import sys
import traceback
import pexpect


def ConnectToYAZServer():
    base_url = "sru.k10plus.de/opac-de-627"
    marc_charset = "MARC8/UTF8"
    output_format = "marc21"

    yaz_client = pexpect.spawn("yaz-client")
    yaz_client.sendline("open " + base_url)
    yaz_client.sendline("marccharset " + marc_charset)
    yaz_client.sendline("format " + output_format)

    return yaz_client


def Main():
    if len(sys.argv) != 3:
        print("[usage]\nissn_lookup.py input_file output_file" +
              "\n\t- input_file should be a text file with one ISSN per line." +
              "\n\t- output_file will be a mrc file with all journals found for theses ISSNs.")
        sys.exit(-1)

    output_file = sys.argv[2]
    input_file = open(sys.argv[1], "r")
    yaz_client = ConnectToYAZServer()
    yaz_client.sendline("set_marcdump " + output_file)
    total_found = 0
    found = 0
    msg = ""
    for line in input_file:
        yaz_client.sendline("find @attr 1=8 " + str.strip(line))
        yaz_client.expect("Number of hits: (\\d+), setno", timeout=1000)
        count_search = re.search(
            b"Number of hits: (\\d+), setno", yaz_client.after)
        if count_search:
            found = int(count_search.group(1))
            total_found += int(count_search.group(1))
        else:
            print('regular expression did not match "' + yaz_client.after + '"!')

        msg = "issn: " + str.strip(line) + " - found: " + \
            str(found) + " record(s), total found: " + \
            str(total_found) + " record(s)\r"

        sys.stdout.write(msg)
        sys.stdout.flush()
        found = 0
        yaz_client.sendline("show all")
        yaz_client.expect("\r\n")

    yaz_client.sendline("exit")

    input_file.close()
    print()  # print linefeed so that progress output doesn't mess up the console


try:
    Main()
except Exception as e:
    print(e + "\n \n" + traceback.format_exc(20))
    sys.exit(-1)
