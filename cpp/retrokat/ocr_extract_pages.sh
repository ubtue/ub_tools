#!/bin/bash

if [ $# != 2 ]; then
  echo "Usage: $0 url image.jpg"
  exit 1
fi

url="$1"
pdf_image="$2"

curl -X POST "https://api.together.xyz/v1/chat/completions" \
  -H "Authorization: Bearer $TOGETHER_API_KEY" \
  -H "Content-Type: application/json" \
  -d @<(cat << EOT 
{
    "model": "Qwen/Qwen2.5-VL-72B-Instruct",
    "messages": [
      {
        "role": "system",
        "content": "You are a text extractor from PDF. Extract the page numbers - they will usually be in the upper left or upper right corner, sometimes lower left or right corner. If the first page has no number, infer it as (page number of next page - 1), but never below 1. Return only the number of the starting page, if no page number is found return null. Respond ONLY with valid JSON: {\"url\": \"user-url\", \"pages\": \"start_page_number\"}. No extra text. Only respond in json."
      },
      {
        "role": "user",
        "content": [
	  {
          "type": "text",
          "text": "Extract the page range for url: $url."
          },
          {
            "type": "image_url",
            "image_url": {
              "url": "data:image/jpeg;base64,$(cat $pdf_image | base64)"
            }
          }
        ]
      }

   ],  
   "stream" : false,
   "temperature" : 0    
 }
EOT
)
