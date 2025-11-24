#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# A tool for the automation of MARC downloads from the BNB.
# This version reads BNB IDs from a PDF file and downloads records from the BNB server.
# Author: Steven Lolong (steven.lolong@uni-tuebingen.de)
# First created: November 2025
# Requires 'pdftotext' utility to parse PDF files.



import pexpect
import re
import os
import sys
import sqlite3
import datetime
import bsz_util

# Global variables
db_connection : sqlite3.Connection
bnb_yaz_client : pexpect.spawn
pdf_file_name : str
marc_output_file : str
tmp_parse_file : str
working_directory : str
email_recipient : str
DATABASE_NAME = "bnb_downloads.db"
CONFIG_FILE = "bnb_config.conf"
LOG_FILE = "log_bnb_downloader_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".log"


# Logging function to log messages (INFO, ERROR, etc.) to a log file
# level: 1 = INFO, 2 = WARNING, 3 = ERROR
def Logging(message, level=1):
    LEVEL_INFO = {1: "INFO", 2: "WARNING", 3: "ERROR"}
    with open(working_directory + LOG_FILE, "a") as log_file:
        log_file.write(LEVEL_INFO.get(level, "INFO") + " -- " + datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S") + " - " + message + "\n")


# Initialize the SQLite database and create necessary tables if they do not exist
def InitializeDatabase():
    UNSUCCESSFUL_DOWNLOAD_TABLE_SCHEMA = '''CREATE TABLE IF NOT EXISTS unsuccessful_download
                            (id INTEGER PRIMARY KEY AUTOINCREMENT,
                            bnb_id TEXT NOT NULL UNIQUE,
                            source_file_name TEXT NOT NULL,
                            date_added TIMESTAMP DEFAULT CURRENT_TIMESTAMP)'''

    SUCCESSFUL_DOWNLOAD_TABLE_SCHEMA = '''CREATE TABLE IF NOT EXISTS successful_download
                            (id INTEGER PRIMARY KEY AUTOINCREMENT,
                            bnb_id TEXT NOT NULL UNIQUE,
                            source_file_name TEXT NOT NULL,
                            date_added TIMESTAMP DEFAULT CURRENT_TIMESTAMP)'''

    try:
        # create a database when it does not exists otherwise connect to the existing one
        with sqlite3.connect(working_directory + DATABASE_NAME) as connection:
            cursor = connection.cursor()
            # create tables if they do not exist
            cursor.execute(UNSUCCESSFUL_DOWNLOAD_TABLE_SCHEMA)
            cursor.execute(SUCCESSFUL_DOWNLOAD_TABLE_SCHEMA)
            return connection
    except sqlite3.Error as e:
        print("An error occurred while initializing the database: " + str(e))
        sys.exit(1)


# Check if the working directory exists
def CheckWorkingDirectory():
    if not os.path.exists(working_directory):
        Logging("The specified working directory does not exist: " + working_directory, 3)
        sys.exit(1)
    

# Get the PDF file name from the configuration file and check if it exists
def GetPDFFileName():
    if(not os.path.exists(working_directory + CONFIG_FILE)):
        Logging("The configuration file does not exist: " + working_directory + CONFIG_FILE, 3)
        sys.exit(1)
    
    with open(working_directory + CONFIG_FILE, "r") as conf_file:
        pdf_file_name = conf_file.read().strip()
        if(not os.path.isfile(working_directory + pdf_file_name)):
            Logging("The specified PDF file in the configuration does not exist: " + working_directory + pdf_file_name, 3)
            sys.exit(1)

    return pdf_file_name


# Connect to the YAZ server
def ConnectToBNBServer():
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


# Insert a BNB ID into the unsuccessful download table
def InsertIdToUnsuccessfulDownloadTable(bnb_id, file_name):
    cursor = db_connection.cursor()
    cursor.execute("SELECT COUNT(*) FROM unsuccessful_download WHERE bnb_id=?", (bnb_id,))
    row = cursor.fetchone()
    if row[0] == 0:
        cursor.execute("INSERT INTO unsuccessful_download (bnb_id, source_file_name) VALUES (?,?)", (bnb_id, file_name))


# Delete a BNB ID from the unsuccessful download table
def DeleteIdFromUnsuccessfulDownloadTable(bnb_id):
    cursor = db_connection.cursor()
    cursor.execute("DELETE FROM unsuccessful_download WHERE bnb_id=?", (bnb_id,))


# Insert a BNB ID into the successful download table
def InsertIdToSuccessfulDownloadTable(bnb_id, file_name):
    cursor = db_connection.cursor()
    cursor.execute("SELECT COUNT(*) FROM successful_download WHERE bnb_id=?", (bnb_id,))
    row = cursor.fetchone()
    if row[0] == 0:
        cursor.execute("INSERT INTO successful_download (bnb_id, source_file_name) VALUES (?,?)", (bnb_id, file_name))


# Check if a BNB ID is in the successful download table
def IsBNBIdInSuccessfulDownloadTable(bnb_id):
    cursor = db_connection.cursor()
    cursor.execute("SELECT COUNT(*) FROM successful_download WHERE bnb_id=?", (bnb_id,))
    row = cursor.fetchone()
    if row[0] > 0:
        return True
    else:
        return False


# Download records for a list of BNB IDs
def DownloadRecordRange(bnb_ids):
    bnb_yaz_client.sendline("format marc21")
    bnb_yaz_client.expect("\r\n")
    bnb_yaz_client.sendline("set_marcdump " + marc_output_file)
    bnb_yaz_client.expect("\r\n")

    counter = 0
    total_found = 0
    total_not_found = 0
    total_existing = 0
    found_ids = []
    not_found_ids = []
    for bnb_id in bnb_ids:
        counter += 1
        if(not IsBNBIdInSuccessfulDownloadTable(bnb_id)):
            bnb_yaz_client.sendline("find @attr 1=48 " + '"' + bnb_id + '"')
            bnb_yaz_client.expect("Number of hits:.*", timeout=1000)
            count_search = re.search(b"Number of hits: (\\d+), setno", bnb_yaz_client.after)
            
            if count_search.group(1).decode("utf-8") != "0":
                bnb_yaz_client.sendline("show all")
                bnb_yaz_client.expect("\r\n")
                total_found += 1
                found_ids.append(bnb_id)
            else:
                not_found_ids.append(bnb_id)
                total_not_found += 1
        else:
            total_existing += 1

    return [found_ids, not_found_ids, total_found, total_not_found, total_existing]


# Retrying previously not found BNB IDs
def RetryingPreviousNotFoundBNBIDs():
    cursor = db_connection.cursor()
    cursor.execute("SELECT bnb_id FROM unsuccessful_download")
    rows = cursor.fetchall()
    previous_not_found_ids = [row[0] for row in rows]
    previous_not_found_ids_pair = {row[0]: row[1] for row in rows}
    total_found = 0
    found_ids = []
    total_previously_not_found = len(previous_not_found_ids)
    if len(previous_not_found_ids) > 0:
        ids_info = DownloadRecordRange(previous_not_found_ids)
        found_ids = ids_info[0]
        total_found = ids_info[2]
        for found_id in found_ids:
            DeleteIdFromUnsuccessfulDownloadTable(found_id)
            InsertIdToSuccessfulDownloadTable(found_id, previous_not_found_ids_pair[found_id])

    progress_info = "Retried downloading Info for previously not found BNB IDs.\n" \
    "Found IDs: " + "\n ".join(found_ids) + "\n"

    Logging(progress_info, 1)
    
    return [total_previously_not_found, total_found]


# Download new BNB IDs from the input PDF file
def DownloadNewBNBIDs(bnb_ids):
    ids_info = DownloadRecordRange(bnb_ids)
    found_ids = ids_info[0]
    not_found_ids = ids_info[1]

    for found_id in found_ids:
        InsertIdToSuccessfulDownloadTable(found_id, pdf_file_name)

    for not_found_id in not_found_ids:
        InsertIdToUnsuccessfulDownloadTable(not_found_id, pdf_file_name)

    progress_info = "Downloaded Info for new BNB IDs from the input PDF file.\n" \
    "Found IDs: " + "\n ".join(found_ids) + "\n" \
    "Not Found IDs: " + "\n ".join(not_found_ids) + "\n"

    Logging(progress_info, 1)

    return [ids_info[2], ids_info[3], ids_info[4]]


# Convert PDF to text and extract BNB IDs
def ExtractBNBIDsFromPDF():
    tmp_pdf_text_file = working_directory + "temp_pdf_text_" + pdf_file_name + ".txt"
    os.system("pdftotext " + working_directory + pdf_file_name + " " + tmp_pdf_text_file)
    bnb_ids = []
    try_to_get = False 
    with open(tmp_pdf_text_file, "r") as pdf_text_file:
        for line in pdf_text_file:
            line = line.strip()
            if line[:5] == "DDC 2":
                try_to_get = True 
                continue

            if line[:10] == "BNB NUMBER" and try_to_get:
                bnb_ids.append(line[11:len(line)])
                try_to_get = False

    os.remove(tmp_pdf_text_file)
    return bnb_ids


def UploadToBSZFTPServer(remote_folder_path: str, marc_filename: str):
    remote_file_name_tmp: str = marc_filename + ".tmp"

    ftp = bsz_util.GetFTPConnection()
    ftp.changeDirectory(remote_folder_path)
    ftp.uploadFile(marc_filename, remote_file_name_tmp)
    ftp.renameFile(remote_file_name_tmp, marc_filename)

# Main function to orchestrate the workflow
def Main():
    if(len(sys.argv) != 3):
        print("usage: " + sys.argv[0] + " <email> <working_directory = "
        )
        sys.exit(-1)

    # It is necessary to declare global variables to modify them inside the function
    global working_directory, pdf_file_name, marc_output_file, email_recipient, db_connection, bnb_yaz_client

    email_recipient = sys.argv[1]
    working_directory = sys.argv[2]
    
    # Check if the working directory exists
    Logging("Checking working directory: " + working_directory, 1)
    CheckWorkingDirectory()
    Logging("Working directory is valid.", 1)

    # Check if the configuration file exists and get the input PDF file name
    Logging("Getting PDF file name from configuration file.", 1)
    pdf_file_name = GetPDFFileName()
    Logging("Input PDF file name: " + pdf_file_name, 1)

    # Prepare other file names
    Logging("Preparing other file names.", 1)
    marc_output_file = "BNB_records_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".mrc"
    Logging("MARC output file name: " + marc_output_file, 1)
    
    # Initialize the database
    Logging("Initializing the database.", 1)
    db_connection = InitializeDatabase()
    Logging("Database initialized.", 1)

    # Connect to BNB server
    Logging("Connecting to BNB server.", 1)
    bnb_yaz_client = ConnectToBNBServer()
    Logging("Connected to BNB server.", 1)

    # Retrying previously not found BNB IDs
    Logging("Retrying previously not found BNB IDs.", 1)
    retrying_download = RetryingPreviousNotFoundBNBIDs()
    Logging("Retried previously not found BNB IDs.", 1)

    # Extract BNB IDs from the input PDF file
    Logging("Extracting BNB IDs from the input PDF file.", 1)
    new_bnb_ids = ExtractBNBIDsFromPDF()
    Logging("Extracted " + str(len(new_bnb_ids)) + " BNB IDs from the input PDF file.", 1)

    # Download new BNB IDs from the input PDF file
    Logging("Downloading new BNB IDs from the input PDF file.", 1)
    new_download = DownloadNewBNBIDs(new_bnb_ids)
    Logging("Downloaded new BNB IDs from the input PDF file.", 1)

    # Upload MARC file to BSZ FTP server
    Logging("Uploading MARC file to BSZ FTP server.", 1)
    UploadToBSZFTPServer("/2001/BNB/input", working_directory + marc_output_file)
    Logging("Uploaded MARC file to BSZ FTP server.", 1)

    # Summary header
    SUMMARY_HEADER = """PDF File Name: {0}
    MARC Output File: {1}
    Working Directory: {2}
    Email Recipient: {3}
    """.format(pdf_file_name, marc_output_file, working_directory, email_recipient)
    Logging(SUMMARY_HEADER, 1)
    print(SUMMARY_HEADER)

    # Summary of the BNB download
    SUMMARY_BODY = """Summary of BNB Download
    Total previously not found BNB IDs: {0}
    Total previously not found BNB IDs now found: {1}
    Total new BNB IDs in the input PDF: {2}
    Total new BNB IDs already existing in the database: {3}
    Total new BNB IDs found: {4}
    Total new BNB IDs not found: {5}
    """.format(retrying_download[0], retrying_download[1], len(new_bnb_ids), new_download[2], new_download[0], new_download[1])
    
    SUMMARY = "----- BNB DOWNLOAD SUMMARY -----\n" + SUMMARY_HEADER + SUMMARY_BODY + "--------------------------------\n"

    Logging(SUMMARY, 1)


    db_connection.commit()
    db_connection.close()
    bnb_yaz_client.sendline("quit")
    bnb_yaz_client.expect(pexpect.EOF)



try:
    Main()
except Exception as e:
    print("An error occurred: " + str(e))
    sys.exit(1)