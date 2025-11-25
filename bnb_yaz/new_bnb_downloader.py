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
DATABASE_NAME = "/usr/local/var/lib/tuelib/bnb_downloads.db"
LOG_FILE = "log_bnb_downloader_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".log"



# Logging function to log messages (INFO, ERROR, etc.) to a log file
# level: 1 = INFO, 2 = WARNING, 3 = ERROR
def Logging(message, level=1):
    LEVEL_INFO = {1: "INFO", 2: "WARNING", 3: "ERROR"}
    with open(working_directory + "logs/" + LOG_FILE, "a") as log_file:
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
        with sqlite3.connect(DATABASE_NAME) as connection:
            cursor = connection.cursor()
            # create tables if they do not exist
            cursor.execute(UNSUCCESSFUL_DOWNLOAD_TABLE_SCHEMA)
            cursor.execute(SUCCESSFUL_DOWNLOAD_TABLE_SCHEMA)
            return connection
    except sqlite3.Error as e:
        Logging("An error occurred while initializing the database: " + str(e), 3)
        sys.exit(1)


# Check if the working directory exists
def CheckWorkingDirectory():
    if not os.path.exists(working_directory):
        Logging("The specified working directory does not exist: " + working_directory, 3)
        sys.exit(1)
    

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
    bnb_yaz_client.sendline("set_marcdump " + working_directory + "marc/" + marc_output_file)
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
                Logging("Skipping BNB ID " + bnb_id + " due to error.", 1)
                Logging("Continuing with next BNB ID.", 1)
                Logging("Writing BNB ID " + bnb_id + " to not found list.", 1)
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
        global marc_output_file
        marc_output_file = "BNB_records_retry_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".mrc"

        ids_info = DownloadRecordRange(previous_not_found_ids)
        found_ids = ids_info[0]
        total_found = ids_info[2]
        for found_id in found_ids:
            DeleteIdFromUnsuccessfulDownloadTable(found_id)
            InsertIdToSuccessfulDownloadTable(found_id, previous_not_found_ids_pair[found_id])

    progress_info = "Retried downloading Info for previously not found BNB IDs.\n"
    
    if len(found_ids) == 0:
        progress_info += "No BNB IDs were found.\n"
    else:
        progress_info += "------------------------------\n" \
            "Total previously not found BNB IDs: " + str(total_previously_not_found) + "\n" \
            "Total found BNB IDs: " + str(total_found) + "\n"\
            "------------------------------\n"
        
    progress_info += "Found IDs: " + "\n".join(found_ids) + "\n"

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

    progress_info = "Downloaded Info for new BNB IDs from the input PDF file.\n"
    if len(found_ids) == 0:
        progress_info += "No BNB IDs were found.\n"
    else:
        progress_info += "------------------------------\n" \
            "From BNB IDs: " + str(len(bnb_ids)) + " in the input PDF file.\n" \
            "Found IDs: \n" + "\n".join(found_ids) + "\n" \
    
    if len(not_found_ids) == 0:
        progress_info += "All BNB IDs were found.\n"
    else:
        progress_info += "------------------------------\n" \
            "From BNB IDs: " + str(len(bnb_ids)) + " in the input PDF file.\n" \
            "Not Found IDs: \n" + "\n".join(not_found_ids) + "\n"

    Logging(progress_info, 1)

    return [ids_info[2], ids_info[3], ids_info[4]]


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


# def UploadToBSZFTPServer(remote_folder_path: str, marc_filename: str):
#     remote_file_name_tmp: str = marc_filename + ".tmp"

#     ftp = bsz_util.GetFTPConnection()
#     ftp.changeDirectory(remote_folder_path)
#     ftp.uploadFile(marc_filename, remote_file_name_tmp)
#     ftp.renameFile(remote_file_name_tmp, marc_filename)


# Get all PDF files in a directory
def GetAllPdfFilesInDirectory(directory: str):
    global pdf_files
    try:
        if not os.path.exists(directory):
            Logging("The specified directory does not exist: " + directory, 3)
            sys.exit(1)
    except Exception as e:
        Logging("An error occurred while checking the directory: " + str(e), 3)
        sys.exit(1)

    for file in os.listdir(directory):
        if file.lower().endswith(".pdf"):
            pdf_files.append(file)


def ProcessingPDFFiles(files: list):
    global pdf_file_name, marc_output_file, total_new_extracted_ids, total_found_ids, total_not_found_ids, total_existing_ids

    progress_info = "Processing PDF files in the directory.\n" \
    "Files to process: \n" + "\n".join(files) + "\n"

    for file in files:
        pdf_file_name = file 

        Logging("\n===============================", 1)
        Logging("Processing PDF file: " + file, 1)
        
        Logging("Preparing MARC output file name.", 1)
        marc_output_file = "BNB_records_" + file[:-4] + "_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".mrc"
        Logging("MARC output file name: " + marc_output_file, 1)
        
        Logging("Extracting BNB IDs from the PDF file.", 1)
        new_bnb_ids = ExtractBNBIDsFromPDF()
        Logging("Extracted " + str(len(new_bnb_ids)) + " BNB IDs from the PDF file.", 1)
        
        Logging("Downloading new BNB IDs from the PDF file.", 1)
        new_download = DownloadNewBNBIDs(new_bnb_ids)
        Logging("Downloaded new BNB IDs from the PDF file.", 1)

        processing_info = "Processed PDF file: " + file + ". Extracting BNB IDs: " + str(len(new_bnb_ids)) + ", Found: " + str(new_download[0]) + ", Not Found: " + str(new_download[1]) + ", Existing: " + str(new_download[2]) + "\n"
        Logging(processing_info, 1)
        progress_info += processing_info

        Logging("Uploading MARC file to BSZ FTP server.", 1)
        # UploadToBSZFTPServer("/2001/BNB/input", working_directory + marc_output_file)
        Logging("Uploaded MARC file to BSZ FTP server.", 1)

        # Update totals
        total_new_extracted_ids += len(new_bnb_ids)
        total_found_ids += new_download[0]
        total_not_found_ids += new_download[1]
        total_existing_ids += new_download[2]

        # move processed PDF file to 'loaded' directory
        os.rename(working_directory + "input/" + file, working_directory + "loaded/" + file)

        os.rename(working_directory + "marc/" + marc_output_file, working_directory + "loaded/" + marc_output_file)
    
    return progress_info



# Main function to orchestrate the workflow
def Main():
    if(len(sys.argv) != 3):
        print("usage: " + sys.argv[0] + " <email> <working_directory>"
        )
        sys.exit(-1)

    # It is necessary to declare global variables to modify them inside the function
    global working_directory,  email_recipient, db_connection, bnb_yaz_client

    email_recipient = sys.argv[1]
    working_directory = sys.argv[2]
    
    # Check if the working directory exists
    Logging("Checking working directory: " + working_directory, 1)
    CheckWorkingDirectory()
    Logging("Working directory is valid.", 1)
    
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

    Logging("Getting all PDF files in the working directory.", 1)
    GetAllPdfFilesInDirectory(working_directory + "input")
    Logging("Obtained all PDF files in the working directory.", 1)
    
    Logging("Processing all PDF files in the directory.", 1)
    progress_info = ProcessingPDFFiles(pdf_files)
    Logging("Processed all PDF files in the directory.", 1)

    # Upload MARC file to BSZ FTP server
    # Logging("Uploading MARC file to BSZ FTP server.", 1)
    # UploadToBSZFTPServer("/2001/BNB/input", working_directory + marc_output_file)
    # Logging("Uploaded MARC file to BSZ FTP server.", 1)



    
    Logging("Closing database connection and BNB server connection.", 1)
    db_connection.commit()
    db_connection.close()
    Logging("Database connection closed.", 1)

    Logging("Closing BNB server connection.", 1)
    bnb_yaz_client.sendline("quit")
    bnb_yaz_client.expect(pexpect.EOF)
    Logging("BNB server connection closed.", 1)

    PROCESSING_INFO = "\n\n" \
    "Processed PDF files: \n" + "\n".join(pdf_files) + "\n\n" + \
    "Previously not found BNB IDs retried: " + str(retrying_download[0]) + "\n" \
    "Previously not found BNB IDs found: " + str(retrying_download[1]) + "\n" \
    "Total new extracted BNB IDs: " + str(total_new_extracted_ids) + "\n" \
    "Total found BNB IDs: " + str(total_found_ids) + "\n" \
    "Total not found BNB IDs: " + str(total_not_found_ids) + "\n" \
    "Total existing BNB IDs: " + str(total_existing_ids) + "\n"

    LOG_SUMMARY = "\n======================================\n" \
    "Per File Download Info.:\n" \
    + progress_info + \
    "--------------------------------------\n" \
    "LOG SUMMARY\n" \
    "======================================\n" \
    + PROCESSING_INFO
    

    EMAIL_BODY = "BNB Download Summary\n\n" + PROCESSING_INFO

    Logging("Finalizing and closing connections.", 1)
    Logging("Final Summary:\n" + LOG_SUMMARY, 1)
    # Send email with log summary
    # os.system('echo "' + LOG_SUMMARY + '" | mail -s "BNB Download Summary" ' + email_recipient)
    Logging("Email sent to " + email_recipient, 1)
    print(EMAIL_BODY)




try:
    Main()
except Exception as e:
    print("An error occurred: " + str(e))
    sys.exit(1)