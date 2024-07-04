#!/bin/python3
# -*- coding: utf-8 -*-
import pyjq
import json
import sys
import util
from openai import OpenAI
import os
import random
import subprocess
import traceback

def SetupChatAI(base_url):
    # API configuration
    api_key = os.environ.get('CHAT_AI_API_KEY')

    if not api_key:
        print("Export CHAT_AI_API_KEY first")
        exit(1)

    # Start OpenAI client
    client = OpenAI(
        api_key = api_key,
        base_url = base_url
    )
    return client



def ProcessRecord(client, model, prompt, item, output_directory, filename):
     query = json.dumps(item)
     chat_completion = client.chat.completions.create(
        messages=[{"role":"system","content": prompt},{"role":"user","content":query}],
        model=model,
     )
     print(json.dumps(item))
     with open(os.path.join(output_directory, filename), 'w') as out:
          out.write(json.dumps(item))
          out.write(chat_completion.choices[0].message.content)
          print(chat_completion.choices[0].message.content)
          out.close()



def Usage():
    print("Usage: " + sys.argv[0] + " rekeyword_directory output_directory")
    sys.exit(-1)


def Main():
    if len(sys.argv) != 3:
        Usage()

    rekeyword_directory = sys.argv[1]
    output_directory = sys.argv[2]

    config=util.LoadConfigFile('./conf/' +  os.path.basename(sys.argv[0])[:-2] + "conf")

    base_url = config.get("Server", "base_url")
    model = config.get("Model", "model")
    promptfile = config.get("Prompt", "promptfile")
    with open(promptfile) as p:
        prompt = p.read()

    client = SetupChatAI(base_url)
    
    rekeyword_abs_path = os.path.abspath(rekeyword_directory)
    for full_file in [entry.path for entry in os.scandir(rekeyword_abs_path) if entry.is_file()]:
        extracted_data = subprocess.run([util.Which("bash") , '-c', \
                         'cat $0 | sed -n \'/XXXXXXXXX/,$p\' | sed 1d | sed -n \'/```/,/```/p\' | sed 1d | sed \'$ d\'', \
                         full_file],
                         stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        if extracted_data.returncode != 0:
            raise ValueError("Extracting JSON from rekeyword file \"" + full_file + "\" did not execute successfully")
        item = pyjq.first('.', json.loads(extracted_data.stdout.decode('utf-8')))
        ProcessRecord(client, model, prompt, item, output_directory, os.path.basename(full_file))


if __name__ == "__main__":
    Main()
