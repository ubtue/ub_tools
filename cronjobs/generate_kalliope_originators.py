#!/bin/python3
# -*- coding: utf-8 -*-
import download_kalliope_originator_records
import extract_kalliope_originators
import os
import re
import shutil
import sys
import tempfile
import traceback
import util
from multiprocessing import Process

def IsResultPlausible(result_file):
    if os.path.getsize(result_file) == 0:
        return False
    with open(result_file) as rfile:
        first_lines = [next(rfile) for x in range(5)]
        correct_line = re.compile('^[0-9\-X]+\s+-\s+.*')
        for line in rfile:
            if not correct_line.match(line):
              return False
    return True
    

def Main():
    tmpdir = tempfile.mkdtemp()
    kalliope_fifo = os.path.join(tmpdir, 'kalliope_fifo')
    print ("Create FIFO: " + kalliope_fifo)
    os.mkfifo(kalliope_fifo)
    RESULT_FILE_NAME = 'kalliope_originators.txt'
    download_process = Process(target=download_kalliope_originator_records.DownloadKalliopeOriginatorRecords, args=[kalliope_fifo]) 
    download_process.start()
    result_file = os.path.join(tmpdir, RESULT_FILE_NAME)
    sys.stdout = open(result_file, 'w')
    extract_process = Process(target=extract_kalliope_originators.ExtractKalliopeOriginators, args=[kalliope_fifo])
    extract_process.start()
    download_process.join()
    extract_process.join()
    sys.stdout.close()
    if not IsResultPlausible(result_file):
        raise("Error - Implausible result file: " + result_file);
    BSZ_DATEN_DIR = '/usr/local/ub_tools/bsz_daten/'
    shutil.move(result_file, BSZ_DATEN_DIR + RESULT_FILE_NAME)
    os.remove(kalliope_fifo)
    os.rmdir(tmpdir)
    util.SendEmail("Generate Kalliope Originators File", "Successfully generated originators file " +
                   BSZ_DATEN_DIR + RESULT_FILE_NAME)
    


try:
    Main()
except Exception as e:
    util.SendEmail("Generate Kalliope Originators File", "An unexpected error occurred: " 
                   + str(e) + "\n\n" + traceback.format_exc(20), priority=1)
