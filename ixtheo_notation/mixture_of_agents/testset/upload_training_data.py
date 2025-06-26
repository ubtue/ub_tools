# Using Python
# based on code from together.ai
from together import Together
import os
import json
import sys

if len(sys.argv) < 2:
      print("Usage: " + sys.argv[0] + "upload_file")
      sys.exit(1)

FILENAME = sys.argv[1]
TOGETHER_API_KEY = os.getenv("TOGETHER_API_KEY")

# Check the file format
from together.utils import check_file

client = Together(api_key=TOGETHER_API_KEY)

sft_report = check_file(FILENAME)
print(json.dumps(sft_report, indent=2))

assert sft_report["is_check_passed"] == True

# Upload the data to Together
train_file_resp = client.files.upload(FILENAME, check=True)
print(train_file_resp.id)  # Save this ID for starting your fine-tuning job
