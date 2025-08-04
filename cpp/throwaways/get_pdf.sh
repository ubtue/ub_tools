#!/bin/bash

function GetPDFLinkFromMainPage {
    local URL="$1"
    # Download the HTML content to a variable
     html_content=$(curl -s "$URL")
     
     # Extract the content attribute of meta tag named citation_pdf_url using grep and sed
     pdf_url=$(echo "$html_content" | grep -oP '(?i)<meta[^>]+name=["'\'']citation_pdf_url["'\''][^>]+content=["'\'']\K[^"'\'']+' | head -1)
     
     if [ -n "$pdf_url" ]; then
       echo "$pdf_url"
     else
       echo "Meta tag 'citation_pdf_url' not found."
       exit 1
     fi
}



# Check if URL is provided
if [ $# != 3 ]; then
  echo "Usage: $0 output_dir PPN URL"
  exit 1
fi

output_dir="$1"
PPN="$2"
URL="$3"
pdf_link_from_main_page=$(GetPDFLinkFromMainPage "$URL")

#sleep .5
echo "Downloading ${PPN}  ${pdf_link_from_main_page}" > /dev/stderr
wget -q -O ${output_dir}/${PPN}.pdf $(echo ${pdf_link_from_main_page})
