#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# A tool for the automation of MARC downloads from the BNB.
# This version reads BNB IDs from a PDF file and downloads records from the BNB server.
# Author: Steven Lolong (steven.lolong@uni-tuebingen.de)
# First created: November 2025
# Usage: new_bnb_downloader.py <email> <working_directory>
# Requires 'pdftotext' utility to parse PDF files.
# The working directory should have the following structure:
# The 'input' directory should contain the PDF files with BNB IDs.
# The 'loaded' directory will store processed PDF files and successfully uploaded MARC files.
# The 'marc' directory will store downloaded MARC files.
# The 'logs' directory will store log files.


import pexpect
import re
import os
import sys
import sqlite3
import datetime
# import bsz_util

# Global variables
db_connection : sqlite3.Connection
bnb_yaz_client : pexpect.spawn
pdf_file_name : str
marc_output_file : str
tmp_parse_file : str
working_directory : str
email_recipient : str
total_new_extracted_ids : int = 0
total_found_ids : int = 0
total_not_found_ids : int = 0
total_existing_ids : int = 0
pdf_files : list = []
new_bnb_info : dict = {}

DATABASE_NAME = "/usr/local/var/lib/tuelib/bnb_downloads.sq3"
LOG_FILE = "log_bnb_downloader_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".log"


# Logging function to log messages (INFO, ERROR, etc.) to a log file
# level: 1 = INFO, 2 = WARNING, 3 = ERROR
def Logging(message, level=1):
    LEVEL_INFO = {1: "INFO", 2: "WARNING", 3: "ERROR"}
    with open(working_directory + "logs/" + LOG_FILE, "a") as log_file:
        log_file.write(LEVEL_INFO.get(level, "INFO") + " -- " + datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S") + " - " + message + "\n")


# Check if the working directory exists
def CheckWorkingDirectory():
    global working_directory
    if not os.path.exists(working_directory):
        Logging("The specified working directory does not exist: " + working_directory, 3)
        # send email about the error
        # bsz_util.SendEmail(email_recipient, "BNB Downloader Error", "The specified working directory does not exist: " + working_directory)
        sys.exit(1)
    else:
        # ensure working_directory ends with a slash
        if not working_directory.endswith("/"):
            working_directory += "/"

        # check for necessary subdirectories
        if not os.path.exists(working_directory + "input/"):
            Logging("The input directory does not exist: " + working_directory + "input/", 3)
            # send email about the error
            # bsz_util.SendEmail(email_recipient, "BNB Downloader Error", "The input directory does not exist: " + working_directory + "input/")
            sys.exit(1)

        # create 'loaded', 'marc', and 'logs' directories if they do not exist
        if not os.path.exists(working_directory + "loaded/"):
            os.makedirs(working_directory + "loaded/")
        if not os.path.exists(working_directory + "marc/"):
            os.makedirs(working_directory + "marc/")
        if not os.path.exists(working_directory + "logs/"):
            os.makedirs(working_directory + "logs/")


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


# Initialize the SQLite database and create necessary tables if they do not exist
def InitializeDatabase():
    RECORDS_TABLE_SCHEMA = '''CREATE TABLE IF NOT EXISTS records
                            (id UNSIGNED BIG INT PRIMARY KEY AUTOINCREMENT,
                            bnb_id TEXT NOT NULL UNIQUE,
                            source_file_name TEXT NOT NULL,
                            date_added TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                            date_delivered TIMESTAMP DEFAULT NULL
                            )'''

    try:
        # create a database when it does not exists otherwise connect to the existing one
        with sqlite3.connect(DATABASE_NAME) as connection:
            cursor = connection.cursor()
            # create tables if they do not exist
            cursor.execute(RECORDS_TABLE_SCHEMA)
            return connection
    except sqlite3.Error as e:
        Logging("An error occurred while initializing the database: " + str(e), 3)
        sys.exit(1)


# Check if a BNB ID is in the successful download table
def IsBNBIdInRecordsTable(bnb_id):
    cursor = db_connection.cursor()
    cursor.execute("SELECT COUNT(*) FROM records WHERE bnb_id=?", (bnb_id))
    row = cursor.fetchone()
    if row[0] > 0:
        return True
    else:
        return False
    
    
# Insert a BNB ID into the records table
def InsertIntoRecords(bnb_id, file_name):
    # insert if not exist
    if not IsBNBIdInRecordsTable(bnb_id):
        cursor = db_connection.cursor()
        cursor.execute("INSERT INTO records(bnb_id, source_file_name) VALUES (?,?)", (bnb_id, file_name))


# Insert a BNB ID into the unsuccessful download table
def UpdateDateDeliveredInRecords(bnb_id):
    cursor = db_connection.cursor()
    # update date_delivered
    cursor.execute("UPDATE records SET date_delivered = ? WHERE bnb_id=?", (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), bnb_id))


# Get BNB IDs for downloading
def GetNotDeliveredIDs():
    cursor = db_connection.cursor()
    cursor.execute("SELECT bnb_id, source_file_name FROM records WHERE date_delivered = NULL ORDER BY source_file_name")
    rows = cursor.fetchall()
    bnb_ids = {row[0]: row[1] for row in rows}
    return bnb_ids



# Download records for a list of BNB IDs
def DownloadRecordRange(bnb_ids):
    bnb_yaz_client.sendline("format marc21")
    bnb_yaz_client.expect("\r\n")
    bnb_yaz_client.sendline("set_marcdump " + working_directory + "marc/" + marc_output_file)
    bnb_yaz_client.expect("\r\n")

    counter = 0
    total_found = 0
    total_not_found = 0
    total_existing = 0
    found_ids = []
    not_found_ids = []
    for bnb_id, source_file in bnb_ids:
        counter += 1
        try:
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
        except Exception as e:
            Logging("An error occurred while downloading BNB ID " + bnb_id + ": " + str(e), 2)
            not_found_ids.append(bnb_id)
            total_not_found += 1
            Logging("Continuing with next BNB ID.", 1)

    return [found_ids, not_found_ids, total_found, total_not_found, total_existing]


# Convert PDF to text and extract BNB IDs
def ExtractBNBIDsFromPDF():
    tmp_pdf_text_file = working_directory + "input/temp_pdf_text_" + pdf_file_name + ".txt"
    os.system("pdftotext " + working_directory + "input/" + pdf_file_name + " " + tmp_pdf_text_file)
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

# Get all PDF files in a directory
def GetAllPdfFilesInDirectory(directory: str):
    global pdf_files
    # check whether the directory exists is not end with a slash
    if not directory.endswith("/"):
        directory += "/"

    for file in os.listdir(directory):
        if file.lower().endswith(".pdf"):
            pdf_files.append(file)


# Processing all PDF files, extract the BNB IDs, and save it to records table
def ProcessingPDFFiles():
    global pdf_file_name, total_new_extracted_ids, total_found_ids, total_not_found_ids, total_existing_ids

    progress_info = "Processing PDF files, extract the BNB IDs, and save it to records table.\n" \
    "Files to process: \n" + "\n".join(pdf_files) + "\n"

    for file in pdf_files:
        pdf_file_name = file

        Logging("Extracting BNB IDs from file: " + file, 1)
        bnb_ids = ExtractBNBIDsFromPDF()
        no_of_id_extracted = len(bnb_ids)
        total_new_extracted_ids = 0
        total_new_extracted_ids_local = 0
        
        if no_of_id_extracted > 0:
            Logging(f"Found: {no_of_id_extracted} new IDs", 1)
            no_of_existing_id = 0
            for bnb_id in bnb_ids:
                if IsBNBIdInRecordsTable(bnb_id):
                    no_of_existing_id += 1
                else:
                    InsertIntoRecords(bnb_id, file)

            total_new_extracted_ids_local += no_of_id_extracted - no_of_existing_id
            total_new_extracted_ids += total_new_extracted_ids_local
            new_bnb_info[file]["no_of_id_extracted"] = no_of_id_extracted 
            new_bnb_info[file]["total_new_extracted_ids"] = total_new_extracted_ids_local
            new_bnb_info[file]["no_of_existing_id"] = no_of_existing_id
            new_bnb_info[file]["new_ids"] = "\n".join(bnb_ids) + "\n"
        else:
            Logging(f"No BNB ID found in the file\n")
            new_bnb_info[file]["no_of_id_extracted"] = 0 
            new_bnb_info[file]["total_new_extracted_ids"] = 0
            new_bnb_info[file]["no_of_existing_id"] = "\n"
            new_bnb_info[file]["new_ids"] = ""
        
        progress_info += f"File: {file} - Extracted IDs: {no_of_id_extracted}, New IDs: {new_bnb_info[file]['total_new_extracted_ids']}, Existing IDs: {new_bnb_info[file]['no_of_existing_id']}\n"
        
        
    
    
    progress_info += "Processing completed.\n"
    Logging(progress_info, 1)
    

# Download undeliverded BNB IDs
def DownloadUndeliveredBNBIDs():
    global marc_output_file, total_found_ids, total_not_found_ids, total_existing_ids

    undelivered_bnb_ids = GetNotDeliveredIDs()
    if len(undelivered_bnb_ids) == 0:
        Logging("No undelivered BNB IDs found.", 1)
        return


    marc_output_file = "bnb_download_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".mrc"
    Logging(f"Downloading undelivered BNB IDs to file: {marc_output_file}", 1)

    [found_ids, not_found_ids, found_count, not_found_count, existing_count] = DownloadRecordRange(undelivered_bnb_ids)

    total_found_ids += found_count
    total_not_found_ids += not_found_count
    total_existing_ids += existing_count

    for bnb_id in found_ids:
        UpdateDateDeliveredInRecords(bnb_id)

    Logging(f"Download completed. Found: {found_count}, Not Found: {not_found_count}, Existing: {existing_count}", 1)
    
def Main():
    # check the command line arguments
    if len(sys.argv) != 3:
        print("Usage: new_bnb_downloader.py <email> <working_directory>")
        # send email about the error
        # bsz_util.SendEmail(email_recipient, "BNB Downloader Error", "Usage: new_bnb_downloader.py <email> <working_directory>")
        sys.exit(1)
    
    # global variables that will be changed
    global working_directory, email_recipient, db_connection, bnb_yaz_client, marc_output_file, new_bnb_info
    
    email_recipient = sys.argv[1]
    working_directory = sys.argv[2]
    
    Logging("Checking working directory...", 1)
    CheckWorkingDirectory()
    Logging("Initializing database...", 1)
    db_connection = InitializeDatabase()
    Logging("Connecting to BNB server...", 1)
    bnb_yaz_client = ConnectToBNBServer()
    Logging("Getting all PDF files in the input directory...", 1)
    GetAllPdfFilesInDirectory(working_directory + "input/")
    Logging("Processing PDF files...", 1)
    ProcessingPDFFiles()

    
    Logging("BNB Downloader completed successfully.", 1)
    Logging("Closing connection to BNB server...", 1)
    bnb_yaz_client.sendline("quit")
    bnb_yaz_client.expect(pexpect.EOF)
    
    Logging("Closing database connection...", 1)
    db_connection.commit()
    db_connection.close()
    
    

try:
    Main()
except Exception as main_exception:
    Logging("An unexpected error occurred: " + str(main_exception), 3)
    # bsz_util.SendEmail(email_recipient, "BNB Downloader Error", "An unexpected error occurred: " + str(main_exception))
    print("An unexpected error occurred: " + str(main_exception))
    # send email about the error
    # bsz_util.SendEmail(email_recipient, "BNB Downloader Error", "An unexpected error occurred: " + str(main_exception))
    sys.exit(1) 
