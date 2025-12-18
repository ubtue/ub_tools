import json
import sys
import requests
import tempfile
import subprocess
import time
import os
import re
from bs4 import BeautifulSoup

if len(sys.argv) != 3:
    print(f"Usage: python {sys.argv[0]} <input_json_urls> <output_json>")
    sys.exit(1)

INPUT_JSON = sys.argv[1]
OUTPUT_JSON = sys.argv[2]

with open(INPUT_JSON, "r", encoding="utf-8") as f:
    article_urls = json.load(f)

if not isinstance(article_urls, list):
    print("Error: Input JSON must be an array of URLs.")
    sys.exit(1)

print(f"Loaded {len(article_urls)} article URLs")

url_to_pages = {}

def run_cmd(cmd):
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed: {cmd}\n{result.stderr}")
    return result.stdout.strip()

def get_pdf_length(pdf_path):
    info = run_cmd(f"pdfinfo '{pdf_path}'")
    for line in info.splitlines():
        if line.startswith("Pages:"):
            return int(line.split(":")[1].strip())
    raise ValueError("Could not determine number of pages")

def convert_first_pages(pdf_path, out_path):
    cmd = f"convert '{pdf_path}[0-2]' -background white -alpha remove -append '{out_path}'"
    run_cmd(cmd)

def query_qwen(url, png_path):
    cmd = f"./ocr_extract_pages.sh '{url}' '{png_path}'"
    output = run_cmd(cmd)

    try:
        data = json.loads(output)
        raw_content = data["choices"][0]["message"]["content"]
    except:
        return {"pages": "0"}

    cleaned = raw_content.strip()
    match = re.search(r'``````', cleaned, re.IGNORECASE)
    if match:
        cleaned = match.group(1).strip()
    cleaned = re.sub(r'\s+', ' ', cleaned).strip()
    
    if not cleaned:
        return {"pages": "0"}

    try:
        parsed = json.loads(cleaned)
        return parsed
    except:
        return {"pages": "0"}


for idx, url in enumerate(article_urls):
    print(f"[{idx+1}/{len(article_urls)}] Processing: {url}")

    try:
        resp = requests.get(url, timeout=10)
        soup = BeautifulSoup(resp.text, "html.parser")

        link = soup.select_one("a.obj_galley_link.pdf")
        if not link or not link.get("href"):
            print("No PDF link found")
            continue

        pdf_url = link['href'].replace("view", "download")
        if not pdf_url.startswith("http"):
            from urllib.parse import urljoin
            pdf_url = urljoin(url, pdf_url)

        with tempfile.TemporaryDirectory() as tmpdir:
            pdf_path = os.path.join(tmpdir, "input.pdf")
            png_path = os.path.join(tmpdir, "preview.png")

            with requests.get(pdf_url, stream=True, timeout=30) as r:
                r.raise_for_status()
                with open(pdf_path, "wb") as f:
                    for chunk in r.iter_content(chunk_size=8192):
                        f.write(chunk)

            pdf_length = get_pdf_length(pdf_path)
            convert_first_pages(pdf_path, png_path)

            extracted = query_qwen(url, png_path)
            start_page = int(extracted.get("pages", 1))
            end_page = start_page + pdf_length - 1

            if (start_page == end_page):
                url_to_pages[url] = f"{start_page}"
            else:
                url_to_pages[url] = f"{start_page}-{end_page}"
            
            print(f"→ {url_to_pages[url]}")

    except Exception as e:
        print(f"Error processing {url}: {e}")

    time.sleep(3)

with open(OUTPUT_JSON, "w", encoding="utf-8") as f:
    json.dump(url_to_pages, f, indent=2, ensure_ascii=False)

print(f"\nResults saved to {OUTPUT_JSON}")
