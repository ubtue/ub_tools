#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# A tool for the automation of MARC downloads from the BNB.
# This version reads BNB IDs from a PDF file and downloads records from the BNB server.
# Author: Steven Lolong (steven.lolong@uni-tuebingen.de)
# First created: November 2025
# Requires 'pdftotext' utility to parse PDF files.
# Requires 'bnb_conf.cnf' file containing the file name of the PDF to be processed.
# Set the path to 'bnb_conf.cnf' in CONFIG_FILE_PATH variable.
# Outputs downloaded MARC records to a file named 'BNB_records_YYYYMMDD_HHMMSS.mrc'.
# Make sure to have a SQLite database 'bnb_info.db' with tables for successful and unsuccessful downloads.
# 



import pexpect
import re
import os
import sys
import sqlite3
import datetime
import bsz_util
import util

CONFIG_FILE_PATH = "/home/iiplo01/Documents/playgrounds/bnb_new_6Oct_25/"
CONFIG_FILE = "bnb_conf.cnf"
with open(CONFIG_FILE_PATH + CONFIG_FILE, "r") as f:
    INPUT_PDF_FILE = f.read()

OUTPUT_MAR_FILE = "BNB_records_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".mrc"
TMP_PARSED_FILE = "temp_parsed_" + INPUT_PDF_FILE + ".txt"

def ParsingPDF():
    os.system('pdftotext -raw ' + INPUT_PDF_FILE + ' ' + TMP_PARSED_FILE)

def ExtractingBNBID():
    bnb_ids = []
    try_to_get = False
    with open(TMP_PARSED_FILE, "r") as file:
        for line in file:
            if(line.strip()[:5] == "DDC 2"):
                try_to_get = True
                continue

            if(line.strip()[:10] == "BNB NUMBER" and try_to_get == True):
                bnb_ids.append(line.strip()[11:len(line.strip())])
                try_to_get = False

    return bnb_ids

def ConnectToYAZServer():
    with open("/usr/local/var/lib/tuelib/bnb_username_password.conf", "r") as f:
            username_password = f.read()        
    yaz_client = pexpect.spawn("yaz-client")
    yaz_client.sendline("auth " + username_password)
    yaz_client.expect("Authentication set to Open.*")
    yaz_client.sendline("open z3950cat.bl.uk:9909")
    yaz_client.expect(".*Connection accepted.*")
    yaz_client.sendline("base BNB03U")
    yaz_client.expect("\r\n")
    yaz_client.sendline("marccharset MARC8/UTF8")
    yaz_client.expect("\r\n")

    return yaz_client

def InsertIdToUnsuccessfulDownloadTable(bnb_id, file_name, connection):
    cursor = connection.cursor()
    cursor.execute("SELECT COUNT(*) FROM unsuccessful_download WHERE bnb_id=?", (bnb_id,))
    row = cursor.fetchone()
    if row[0] == 0:
        cursor.execute("INSERT INTO unsuccessful_download (bnb_id, source_file_name, timestamp) VALUES (?,?,?)", (bnb_id, file_name, '{:%Y-%m-%d %H:%M:%S}'.format(datetime.datetime.now())))

def DeleteIdFromUnsuccessfulDownloadTable(bnb_id, connection):
    cursor = connection.cursor()
    cursor.execute("DELETE FROM unsuccessful_download WHERE bnb_id=?", (bnb_id,))

def InsertIdToSuccessfulDownloadTable(bnb_id, file_name, connection):
    cursor = connection.cursor()
    cursor.execute("SELECT COUNT(*) FROM successful_download WHERE bnb_id=?", (bnb_id,))
    row = cursor.fetchone()
    if row[0] == 0:
        cursor.execute("INSERT INTO successful_download (bnb_id, source_file_name, timestamp) VALUES (?,?,?)", (bnb_id, file_name, '{:%Y-%m-%d %H:%M:%S}'.format(datetime.datetime.now())))

def IsBNBIdInSuccessfulDownloadTable(bnb_id, connection):
    cursor = connection.cursor()
    cursor.execute("SELECT COUNT(*) FROM successful_download WHERE bnb_id=?", (bnb_id,))
    row = cursor.fetchone()
    if row[0] > 0:
        return True
    else:
        return False

def DownloadRecordRange(yaz_client, bnb_ids, connection):
    yaz_client.sendline("format marc21")
    yaz_client.expect("\r\n")
    yaz_client.sendline("set_marcdump " + OUTPUT_MAR_FILE)
    yaz_client.expect("\r\n")

    counter = 0
    total_found = 0
    total_not_found = 0
    total_existing = 0
    found_ids = []
    not_found_ids = []
    for bnb_id in bnb_ids:
        counter += 1
        if(not IsBNBIdInSuccessfulDownloadTable(bnb_id, connection)):
            yaz_client.sendline("find @attr 1=48 " + '"' + bnb_id + '"')
            yaz_client.expect("Number of hits:.*", timeout=1000)
            count_search = re.search(b"Number of hits: (\\d+), setno", yaz_client.after)
            
            if count_search.group(1).decode("utf-8") != "0":
                yaz_client.sendline("show all")
                yaz_client.expect("\r\n")
                found_ids.append(bnb_id)
                total_found += 1
            else:
                not_found_ids.append(bnb_id)
                total_not_found += 1
        else:
            total_existing += 1

    return [found_ids, not_found_ids, total_found, total_not_found, total_existing]

def RetryingPreviousNotFoundBNBIDs(yaz_client, connection):
    cursor = connection.cursor()
    cursor.execute("SELECT bnb_id, source_file_name FROM unsuccessful_download")
    rows = cursor.fetchall()

    bnb_ids_not_available_on_server = [row[0] for row in rows]
    bnb_ids_not_available_on_server_pair = {row[0]: row[1] for row in rows}
    total_found = 0
    total_previously_not_found = len(bnb_ids_not_available_on_server)
    if len(bnb_ids_not_available_on_server) > 0:
        ids_info = DownloadRecordRange(yaz_client, bnb_ids_not_available_on_server, connection)
        found_ids = ids_info[0]
        total_found = ids_info[2]
        for found_id in found_ids:
            DeleteIdFromUnsuccessfulDownloadTable(found_id, connection)
            InsertIdToSuccessfulDownloadTable(found_id, bnb_ids_not_available_on_server_pair[found_id], connection)

    return [total_previously_not_found, total_found]

    
def DownloadNewBNBIDs(yaz_client, bnb_ids, connection):
    ids_info = DownloadRecordRange(yaz_client, bnb_ids, connection)
    found_ids = ids_info[0]
    not_found_ids = ids_info[1]

    for found_id in found_ids:
        InsertIdToSuccessfulDownloadTable(found_id, INPUT_PDF_FILE, connection)

    for not_found_id in not_found_ids:
        InsertIdToUnsuccessfulDownloadTable(not_found_id, INPUT_PDF_FILE, connection)

    return [ids_info[2], ids_info[3], ids_info[4]]

def UploadToBSZFTPServer(remote_folder_path: str, marc_filename: str):
    remote_file_name_tmp: str = marc_filename + ".tmp"

    ftp = bsz_util.GetFTPConnection()
    ftp.changeDirectory(remote_folder_path)
    ftp.uploadFile(marc_filename, remote_file_name_tmp)
    ftp.renameFile(remote_file_name_tmp, marc_filename)

def Main():
    if len(sys.argv) != 2:
        print("usage: " + sys.argv[0] + "<email>")
        util.SendEmail("BNB Downloader Invocation Failure",
                        "This script needs to be called with one argument, the email address!\n", priority=1)
        sys.exit(-1)

    email_recipient = sys.argv[1]
    connection = sqlite3.connect(CONFIG_FILE_PATH + "bnb_info.db")
    
    print("Connecting to BNB Server")
    yaz_client = ConnectToYAZServer()

    print("Retrying download previous not found BNB IDs from BNB Server")
    retrying_download = RetryingPreviousNotFoundBNBIDs(yaz_client, connection)

    print("Parsing PDF")
    ParsingPDF()

    print("Extracting BNB ID")
    new_bnb_ids = ExtractingBNBID()

    print("Downloading Records from BNB Server")
    new_download = DownloadNewBNBIDs(yaz_client, new_bnb_ids, connection)

    BNB_DOWNLOAD_SUMMARY = """Summary of BNB Download
Total previously not found BNB IDs: {0}
Total previously not found BNB IDs now found: {1}
Total new BNB IDs in the input PDF: {2}
Total new BNB IDs already existing in the database: {3}
Total new BNB IDs found: {4}
Total new BNB IDs not found: {5}
""".format(retrying_download[0], retrying_download[1], len(new_bnb_ids), new_download[2], new_download[0], new_download[1])

    if new_download[2] > 0:
        print("Uploading MARC file to BSZ FTP Server")
        FTP_UPLOAD_DIRECTORY: str = "/2001/BNB/input"

        UploadToBSZFTPServer(FTP_UPLOAD_DIRECTORY, OUTPUT_MAR_FILE)

    try:
        print("Removing temporary parsed file: " + TMP_PARSED_FILE)
        os.remove(TMP_PARSED_FILE)
    except OSError:
        pass

    yaz_client.sendline("quit")
    yaz_client.expect(pexpect.EOF)
    connection.commit()
    connection.close()

    print(BNB_DOWNLOAD_SUMMARY)
    util.SendEmail("BNB Download Summary", BNB_DOWNLOAD_SUMMARY, recipient=email_recipient, priority=1)


try:
    Main()
except Exception as e:
    print(e)
