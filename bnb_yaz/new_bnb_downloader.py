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


import pexpect
import re
import os
import sys
import sqlite3
import datetime
import util
import bsz_util

# Global variables
db_connection : sqlite3.Connection
bnb_yaz_client : pexpect.spawn
working_directory : str
email_recipient : str
total_id_extracted : int = 0
set_of_existing_ids : set = set()
set_of_new_ids : set = set()
number_of_downloaded_ids : int = 0
pdf_files : list = []
new_bnb_info : dict = {}

CONFIG_DIRECTORY_PATH = "/usr/local/var/lib/tuelib/"
DATABASE_NAME = CONFIG_DIRECTORY_PATH + "bnb_downloads.sq3"

# initialize new_bnb_info's key structure
def InitializeNewBNBInfoStructure(key: str):
    global new_bnb_info
    if key not in new_bnb_info:
        new_bnb_info[key] = {
            "list_of_id_extracted": list(),
            "list_of_existing_id": list(),
            "list_of_new_id": list(),
            "list_of_not_found_id": list(),
            "list_of_error_id": list(),
            "list_of_downloaded_id": list(),
        }


# Check if the working directory exists
def CheckWorkingDirectory():
    global working_directory
    if not os.path.exists(working_directory):
        util.Error("The specified working directory does not exist: " + working_directory)
    else:
        # ensure working_directory ends with a slash
        if not working_directory.endswith("/"):
            working_directory += "/"

        # check for necessary subdirectories
        if not os.path.exists(working_directory + "input/"):
            util.Error("The input directory does not exist: " + working_directory + "input/")

        # create 'loaded', 'marc', and 'logs' directories if they do not exist
        if not os.path.exists(working_directory + "loaded/"):
            os.makedirs(working_directory + "loaded/")
        if not os.path.exists(working_directory + "marc/"):
            os.makedirs(working_directory + "marc/")


# Connect to the YAZ server
def ConnectToBNBServer():
    with open(CONFIG_DIRECTORY_PATH + "bnb_username_password.conf", "r") as f:
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
                            (id  INTEGER PRIMARY KEY AUTOINCREMENT,
                            bnb_id TEXT NOT NULL UNIQUE,
                            source_file_name TEXT NOT NULL,
                            date_added TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                            date_delivered TIMESTAMP DEFAULT NULL
                            )'''

    with sqlite3.connect(DATABASE_NAME) as connection:
        cursor = connection.cursor()
        # create tables if they do not exist
        cursor.execute(RECORDS_TABLE_SCHEMA)
        return connection


# Check if a BNB ID is in the successful download table
def IsBNBIdInRecordsTable(bnb_id):
    cursor = db_connection.cursor()
    cursor.execute("SELECT COUNT(*) FROM records WHERE bnb_id='" + bnb_id + "'")
    row = cursor.fetchone()
    if row[0] > 0:
        return True
    else:
        return False
    
    
# Insert a BNB ID into the records table when not exist
def InsertIntoRecordsIfNotExist(bnb_id, file_name):
    global new_bnb_info
    # insert if not exist
    if not IsBNBIdInRecordsTable(bnb_id):
        cursor = db_connection.cursor()
        cursor.execute("INSERT INTO records(bnb_id, source_file_name) VALUES (?,?)", (bnb_id, file_name))
    else:
        # check whether the key "list_of_existing_id" exists in new_bnb_info for the file_name
        if file_name not in new_bnb_info:
            InitializeNewBNBInfoStructure(file_name)

        new_bnb_info[file_name]["list_of_existing_id"].append(bnb_id)


# Insert a BNB ID into the unsuccessful download table
def UpdateDateDeliveredInRecords(bnb_id):
    cursor = db_connection.cursor()
    # update date_delivered
    cursor.execute("UPDATE records SET date_delivered = ? WHERE bnb_id=?", (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), bnb_id))


# Get BNB IDs for downloading
def GetNotDeliveredIDs():
    cursor = db_connection.cursor()
    # select bnb_id and source_file_name where date_delivered is NULL
    cursor.execute("SELECT bnb_id, source_file_name FROM records WHERE date_delivered IS NULL")
    rows = cursor.fetchall()
    bnb_ids = []
    
    for row in rows:
        bnb_ids.append((row[0], row[1]))
    
    return bnb_ids


# Download records for a list of BNB IDs
def DownloadRecordRange(bnb_ids):
    global marc_output_file, new_bnb_info, number_of_downloaded_ids
    bnb_yaz_client.sendline("format marc21")
    bnb_yaz_client.expect("\r\n")

    counter = 0
    source_file_name = ""
    for bnb_id, source_file in bnb_ids:
        # create new entry in new_bnb_info if not exist
        if source_file not in new_bnb_info:
            InitializeNewBNBInfoStructure(source_file)
        
        # group the output marc file by source file name
        if( source_file != source_file_name):
            source_file_name = source_file
            marc_output_file = "bnb-" + datetime.datetime.now().strftime("%y%m%d") + "-" + source_file_name + ".mrc"
            bnb_yaz_client.sendline("set_marcdump " + working_directory + "marc/" + marc_output_file)
            bnb_yaz_client.expect("\r\n")
            
        counter += 1
        try:
            bnb_yaz_client.sendline("find @attr 1=48 " + '"' + bnb_id + '"')
            bnb_yaz_client.expect("Number of hits:.*", timeout=1000)
            count_search = re.search(b"Number of hits: (\\d+), setno", bnb_yaz_client.after)
            
            if count_search.group(1).decode("utf-8") != "0":
                bnb_yaz_client.sendline("show all")
                bnb_yaz_client.expect("\r\n")

                number_of_downloaded_ids += 1
                new_bnb_info[source_file]["list_of_downloaded_id"].append(bnb_id)
                UpdateDateDeliveredInRecords(bnb_id)
            else:
                new_bnb_info[source_file]["list_of_not_found_id"].append(bnb_id)
        except Exception as e:
            util.Info("An error occurred while downloading BNB ID " + bnb_id + ": " + str(e))
            new_bnb_info[source_file]["list_of_error_id"].append(bnb_id)


# Convert PDF to text and extract BNB IDs
def ExtractBNBIDsFromPDF(pdf_file_name):
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
    global total_id_extracted, new_bnb_info, set_of_existing_ids, set_of_new_ids

    for file in pdf_files:
        util.Info("Processing file: " + file)
        # initialize new_bnb_info for the file if not exist
        if file not in new_bnb_info:
            InitializeNewBNBInfoStructure(file)
        
        # extract BNB IDs from the PDF file
        bnb_ids = ExtractBNBIDsFromPDF(file)
        
        new_bnb_info[file]["list_of_id_extracted"] = bnb_ids
        
        total_id_extracted += len(bnb_ids)
        
        if len(new_bnb_info[file]["list_of_id_extracted"]) > 0:
            util.Info(f"Extract: {len(new_bnb_info[file]['list_of_id_extracted'])} IDs")
            for bnb_id in bnb_ids:
                if IsBNBIdInRecordsTable(bnb_id):
                    new_bnb_info[file]["list_of_existing_id"].append(bnb_id)
                    set_of_existing_ids.add(bnb_id)
                else:
                    InsertIntoRecordsIfNotExist(bnb_id, file)
                    set_of_new_ids.add(bnb_id)
                    if "list_of_new_id" not in new_bnb_info[file]:
                        new_bnb_info[file]["list_of_new_id"] = list()
                        
                    new_bnb_info[file]["list_of_new_id"].append(bnb_id)
        else:
            new_bnb_info[file]["list_of_id_extracted"] = []
            new_bnb_info[file]["list_of_existing_id"] = []
            new_bnb_info[file]["list_of_new_id"] = []
        
        # move processed file to loaded directory
        os.rename(working_directory + "input/" + file, working_directory + "loaded/" + file)
        util.Info("Moved processed file to loaded directory: " + working_directory + "loaded/" + file)


# Upload MARC files to FTP server
def UploadMARCFilesToBSZFTPServer(remote_directory: str):
    sftp = bsz_util.GetFTPConnection("SFTP_Upload")
    MARC_FILE_DIRECTORY = working_directory + "marc/"
    for file in os.listdir(MARC_FILE_DIRECTORY):
        if file.lower().endswith(".mrc"):
            file_with_path = MARC_FILE_DIRECTORY + file
            # check if the file size is greater than 0
            if os.path.getsize(file_with_path) > 0:
                util.Info("Uploading MARC file to BSZ FTP server: " + file)
                sftp.changeDirectory(remote_directory)
                sftp.uploadFile(file_with_path, file + ".tmp")
                sftp.renameFile(file + ".tmp", file)
                util.Info("Successfully uploaded MARC file: " + file)
                # move uploaded MARC file to loaded directory
                os.rename(file_with_path, working_directory + "loaded/" + file)
                util.Info("Moved uploaded MARC file to loaded directory: " + working_directory + "loaded/" + file)
                
    sftp.close()
    

# Write summary report
def WriteSummaryReport() -> str:
    summary_report = "Detailed Summary Report of BNB Downloader\n"
    summary_report += "=================================\n\n"
    for file_name, info in new_bnb_info.items():
        summary_report += f"File: {file_name}\n"
        summary_report += f"  Extracted IDs: {len(info['list_of_id_extracted'])}\n"
        summary_report += f"  Existing IDs: {len(info['list_of_existing_id'])}\n"
        summary_report += f"  New IDs: {len(info['list_of_new_id'])}\n"
        summary_report += f"  List of Not Found IDs: {', '.join(info['list_of_not_found_id'])}\n"
        summary_report += f"  Not Found IDs: {len(info['list_of_not_found_id'])}\n"
        summary_report += f"  List of Error IDs: {', '.join(info['list_of_error_id'])}\n"
        summary_report += f"  Error IDs: {len(info['list_of_error_id'])}\n"
        summary_report += f"  Downloaded IDs: {len(info['list_of_downloaded_id'])}\n\n"
        
    summary_report += "BNB Downloader Summary Report\n"
    summary_report += "=================================\n\n"
    summary_report += f"List of processed PDF files: {', '.join(pdf_files)}\n"
    summary_report += f"Total PDF files processed: {len(pdf_files)}\n"
    summary_report += f"Total BNB IDs extracted: {total_id_extracted}\n"
    summary_report += f"Total new BNB IDs to download: {len(set_of_new_ids)}\n"
    summary_report += f"Total existing BNB IDs: {len(set_of_existing_ids)}\n"
    summary_report += f"Total of not found BNB IDs: {sum(len(info['list_of_not_found_id']) for info in new_bnb_info.values())}\n"
    summary_report += f"Total of error BNB IDs: {sum(len(info['list_of_error_id']) for info in new_bnb_info.values())}\n"
    summary_report += f"Total downloaded BNB IDs: {number_of_downloaded_ids}\n\n"
    
    
    return summary_report
    
    
    
# Main function
def Main():
    # check the command line arguments
    if len(sys.argv) != 3:
        util.Error("Invalid number of arguments. Usage: new_bnb_downloader.py <email> <working_directory>")
    
    # global variables that will be changed
    global working_directory, email_recipient, db_connection, bnb_yaz_client
    
    email_recipient = sys.argv[1]
    working_directory = sys.argv[2]
    
    # set default email recipient in util module, needed for util.SendEmail() when error occurs
    util.default_email_recipient = email_recipient
    
    util.Info("Starting BNB Downloader...")
    
    CheckWorkingDirectory()
    
    util.Info("Initializing database...")
    db_connection = InitializeDatabase()
    
    util.Info("Connecting to BNB server...")
    bnb_yaz_client = ConnectToBNBServer()
    
    util.Info("Getting all PDF files in the input directory...")
    GetAllPdfFilesInDirectory(working_directory + "input/")
    
    util.Info("Processing PDF files...")
    ProcessingPDFFiles()
    
    util.Info("Getting undelivered BNB IDs...")
    undelivered_bnb_ids = GetNotDeliveredIDs()
    if len(undelivered_bnb_ids) > 0:
        util.Info("Downloading undelivered BNB IDs...")
        DownloadRecordRange(undelivered_bnb_ids)
    else:
        util.Info("No undelivered BNB IDs found.")
    
    util.Info("BNB Downloader completed successfully.")
    
    util.Info("Closing connection to BNB server...")
    bnb_yaz_client.sendline("quit")
    bnb_yaz_client.expect(pexpect.EOF)
    
    util.Info("Closing database connection...")
    db_connection.commit()
    db_connection.close()
    
    util.Info("Uploading MARC files to BSZ FTP server...")
    UploadMARCFilesToBSZFTPServer("/2001/BNB_Test/input")

    # write summary log
    util.Info("Writing summary report...")
    summary_report = WriteSummaryReport()
    util.Info(summary_report)
    
    # send summary report via email
    util.Info("Sending summary report via email to " + email_recipient + "...")
    util.SendEmail("BNB Downloader Summary Report", summary_report, recipient=email_recipient)
    

try:
    Main()
except Exception as main_exception:
    util.Error("An unexpected error occurred: " + str(main_exception))
