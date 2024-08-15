#!/bin/python3
# -*- coding: utf-8 -*-
import pyjq
import json
import sys
import util
from openai import OpenAI
import os
import random
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



def ReadFulltext(file_path):
    with open(file_path) as json_data:
        fulltext_parsed = json.load(json_data)
        return fulltext_parsed


def ProcessRecord(client, model, prompt, item, output_directory):
     query = json.dumps(item)
     chat_completion = client.chat.completions.create(
        messages=[{"role":"system","content": prompt},{"role":"user","content":query}],
        model=model,
     )
     with open(os.path.join(output_directory, item['id']), 'w') as out:
          print(json.dumps(item))
          out.write(json.dumps(item))
          out.write("\nXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n")
          out.write(chat_completion.choices[0].message.content)
          print(chat_completion.choices[0].message.content)
          out.close()


def GetRandomOffsets(num_of_samples, fulltext_parsed):
    num_of_elements = pyjq.first('length', fulltext_parsed)
    random_elements = list(random.sample(range(0, num_of_elements-1), num_of_samples))
    return '.' + str(random_elements)


def Usage():
    print("Usage: " + sys.argv[0] + " [--sample number_of_samples] augmented_tocs.json output_directory")
    sys.exit(-1)


def Main():
    use_samples = False

    if len(sys.argv) != 3 and len(sys.argv) != 5:
        Usage()

    if len(sys.argv) == 5:
        optional_parameter = sys.argv.pop(1)
        if optional_parameter != '--sample':
            Usage()
        use_samples = True
    
    if len(sys.argv) == 4:
        number_of_samples = int(sys.argv.pop(1))
        if not isinstance(number_of_samples, int) or not (number_of_samples > 0):
            Usage()

    augmented_tocs = sys.argv[1]
    output_directory = sys.argv[2]

    config=util.LoadConfigFile('./conf/' +  os.path.basename(sys.argv[0])[:-2] + "conf")

    jq_array_element_selector = '.[]'
    if use_samples:
       jq_array_element_selector = GetRandomOffsets(number_of_samples, ReadFulltext(augmented_tocs))

    base_url = config.get("Server", "base_url")
    model = config.get("Model", "model")
    prompt = config.get("Prompt", "prompt")

    client = SetupChatAI(base_url)

    items = pyjq.all(jq_array_element_selector, ReadFulltext(augmented_tocs))
    for item in items:
        if item['id'] == '894026267':
            continue
        print("Processing item " + item['id']);
        ProcessRecord(client, model, prompt, item, output_directory)


if __name__ == "__main__":
    Main()
