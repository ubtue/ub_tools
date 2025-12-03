#!/bin/bash


if [ $# != 1 ]; then
  echo "Usage: $0 /path/to/folder"
  exit 1
fi


dir="$1"

all_content=$(cat "$dir"/* | jq -Rs)

together_json=$(cat <<EOF
 {
    "model": "moonshotai/Kimi-K2-Thinking",
    "messages": [
      {
        "role": "system",
        "content": "You merge metadata from multiple input JSON files into an array of valid Zotero JSON objects, each representing a distinct journal article. Each article must be clearly separated as an object in the output array. Combine metadata from the issue page with the metadata from the individual article files, ensuring the output strictly adheres to the Zotero JSON format for a journal article: {\n\t\"itemType\": \"journalArticle\",\n\t\"title\": \"Title\",\n\t\"abstractNote\": \"Abstract\",\n\t\"publicationTitle\": \"Publication\",\n\t\"volume\": \"Volume\",\n\t\"issue\": \"Issue\",\n\t\"pages\": \"Pages\",\n\t\"date\": \"Date\",\n\t\"series\": \"Series\",\n\t\"seriesTitle\": \"Series Title\",\n\t\"seriesText\": \"Series Text\",\n\t\"journalAbbreviation\": \"Journal Abbr\",\n\t\"language\": \"Language\",\n\t\"DOI\": \"DOI\",\n\t\"ISSN\": \"ISSN\",\n\t\"shortTitle\": \"Short Title\",\n\t\"url\": \"URL\",\n\t\"accessDate\": \"Accessed\",\n\t\"archive\": \"Archive\",\n\t\"archiveLocation\": \"Loc. in Archive\",\n\t\"libraryCatalog\": \"Library Catalog\",\n\t\"callNumber\": \"Call Number\",\n\t\"rights\": \"Rights\",\n\t\"extra\": \"Extra\",\n\t\"creators\": [\n\t\t{\n\t\t\t\"firstName\": \"\",\n\t\t\t\"lastName\": \"\",\n\t\t\t\"creatorType\": \"author\",\n\t\t\t\"fieldMode\": 1\n\t\t},\n\t\t{\n\t\t\t\"firstName\": \"\",\n\t\t\t\"lastName\": \"\",\n\t\t\t\"creatorType\": \"contributor\",\n\t\t\t\"fieldMode\": 1\n\t\t},\n\t\t{\n\t\t\t\"firstName\": \"\",\n\t\t\t\"lastName\": \"\",\n\t\t\t\"creatorType\": \"editor\",\n\t\t\t\"fieldMode\": 1\n\t\t},\n\t\t{\n\t\t\t\"firstName\": \"\",\n\t\t\t\"lastName\": \"\",\n\t\t\t\"creatorType\": \"reviewedAuthor\",\n\t\t\t\"fieldMode\": 1\n\t\t},\n\t\t{\n\t\t\t\"firstName\": \"\",\n\t\t\t\"lastName\": \"\",\n\t\t\t\"creatorType\": \"translator\",\n\t\t\t\"fieldMode\": 1\n\t\t}\n\t],\n\t\"attachments\": [\n\t\t{\n\t\t\t\"url\": \"\",\n\t\t\t\"document\": {},\n\t\t\t\"title\": \"\",\n\t\t\t\"mimeType\": \"\",\n\t\t\t\"snapshot\": false\n\t\t}\n\t],\n\t\"tags\": [\n\t\t{\n\t\t\t\"tag\": \"\"\n\t\t}\n\t],\n\t\"notes\": [\n\t\t{\n\t\t\t\"note\": \"\"\n\t\t}\n\t],\n\t\"seeAlso\": []\n.}. The url gathered from the issue page is very important - make sure every article has a url associated with it. The Orcid should stay as a note in the format: 'orcid:{orcid} | {author_name}'. If an article has section 'Reviews & Notices' or the title includes 'Book Review', add a tag with the value 'Book Review'. Ensure no articles or metadata are missed. Only respond in json."
      },
      {
        "role": "user",
        "content": $all_content
      }
    ],
    "stream": false,
    "max_tokens": 14000,
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
# cat <(echo $together_json)

