#!/bin/bash


if [ $# != 1 ]; then
  echo "Usage: $0 url"
  exit 1
fi

url="$1"

page=$(curl -s $url | sed '/<script\b/,/<\/script>/d' | jq -Rs .)
#page=$(curl -s $url | jq -Rs .)
#page="HALLO"

together_json=$(cat <<EOF
{
    "model": "moonshotai/Kimi-K2-Thinking",
    "messages": [
      {
        "role": "system",
        "content": "Please extract all the article references from the following web page code paying attention to the sections they are in. For each article include volume, issue and year from the page top, the section in the page. Make sure you also include given links if present. Only respond in JSON. Generate a JSON object with keys title, author, volume, issue, year, section, url. Return the amount of found articles at the top. Double check not to leave out any articles"
      },
      {
        "role": "user",
	"content" : $page
      }
    ],
    "stream": false,
    "max_tokens": 13936,
    "temperature": 0
  }
EOF
)

curl -X POST "https://api.together.xyz/v1/chat/completions" \
 -H "Authorization: Bearer $TOGETHER_API_KEY" \
 -H "Content-Type: application/json" \
 -d @<(echo $together_json) #|  \
#jq .choices[].message.content | \
#awk -F '```json|```' '{print $2}' | \
#sed -re 's/\\[nt]//g' | grep -v '^$' | \
#sed -re 's/(.*)/"\1"/' | jq -ra fromjson
