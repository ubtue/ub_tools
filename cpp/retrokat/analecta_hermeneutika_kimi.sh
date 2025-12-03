#!/bin/bash


if [ $# != 1 ]; then
  echo "Usage: $0 stripped.md"
  exit 1
fi
 


stripped_markdown="$1"

together_json=$(cat <<EOF
 {
    "model": "moonshotai/Kimi-K2-Thinking",
    "messages": [
      {
        "role": "system",
        "content": "You extract metadata from a pdf converted to text. You get the first, second and last page of an article. Extract author, title, orcid, name of the journal, abstract, url, keyword if present and determine the page range from the information given. Calculate the page range from the page information given on the last page and on the first or second page. Notice that the first page might not contain a page number. In these cases take the second page as a starting point and subtract one to derive the correct page range. If the first page contains only a preview do not include it in the page rage. The page range is very important. Only extract an abstract if it is clearly specified as such. Write the orcid into a notes field, also including the corresponding author name in the format 'orcid:{orcid} | {author_name}'. Add a new note for every orcid and author combination. Generate a JSON object as output and stick to Zotero JSON format: {\n\t\"itemType\": \"journalArticle\",\n\t\"title\": \"Title\",\n\t\"abstractNote\": \"Abstract\",\n\t\"publicationTitle\": \"Publication\",\n\t\"volume\": \"Volume\",\n\t\"issue\": \"Issue\",\n\t\"pages\": \"Pages\",\n\t\"date\": \"Date\",\n\t\"series\": \"Series\",\n\t\"seriesTitle\": \"Series Title\",\n\t\"seriesText\": \"Series Text\",\n\t\"journalAbbreviation\": \"Journal Abbr\",\n\t\"language\": \"Language\",\n\t\"DOI\": \"DOI\",\n\t\"ISSN\": \"ISSN\",\n\t\"shortTitle\": \"Short Title\",\n\t\"url\": \"URL\",\n\t\"accessDate\": \"Accessed\",\n\t\"archive\": \"Archive\",\n\t\"archiveLocation\": \"Loc. in Archive\",\n\t\"libraryCatalog\": \"Library Catalog\",\n\t\"callNumber\": \"Call Number\",\n\t\"rights\": \"Rights\",\n\t\"extra\": \"Extra\",\n\t\"creators\": [\n\t\t{\n\t\t\t\"firstName\": \"\",\n\t\t\t\"lastName\": \"\",\n\t\t\t\"creatorType\": \"author\",\n\t\t\t\"fieldMode\": 1\n\t\t},\n\t\t{\n\t\t\t\"firstName\": \"\",\n\t\t\t\"lastName\": \"\",\n\t\t\t\"creatorType\": \"contributor\",\n\t\t\t\"fieldMode\": 1\n\t\t},\n\t\t{\n\t\t\t\"firstName\": \"\",\n\t\t\t\"lastName\": \"\",\n\t\t\t\"creatorType\": \"editor\",\n\t\t\t\"fieldMode\": 1\n\t\t},\n\t\t{\n\t\t\t\"firstName\": \"\",\n\t\t\t\"lastName\": \"\",\n\t\t\t\"creatorType\": \"reviewedAuthor\",\n\t\t\t\"fieldMode\": 1\n\t\t},\n\t\t{\n\t\t\t\"firstName\": \"\",\n\t\t\t\"lastName\": \"\",\n\t\t\t\"creatorType\": \"translator\",\n\t\t\t\"fieldMode\": 1\n\t\t}\n\t],\n\t\"attachments\": [\n\t\t{\n\t\t\t\"url\": \"\",\n\t\t\t\"document\": {},\n\t\t\t\"title\": \"\",\n\t\t\t\"mimeType\": \"\",\n\t\t\t\"snapshot\": false\n\t\t}\n\t],\n\t\"tags\": [\n\t\t{\n\t\t\t\"tag\": \"\"\n\t\t}\n\t],\n\t\"notes\": [\n\t\t{\n\t\t\t\"note\": \"\"\n\t\t}\n\t],\n\t\"seeAlso\": []\n.} Only respond in json."
      },
      {
        "role": "user",
	"content": $(cat $stripped_markdown | jq -Rs)
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
 -d @<(echo $together_json) |  \
jq .choices[].message.content | \
awk -F '```json|```' '{print $2}' | \
sed -re 's/\\[nt]//g' | grep -v '^$' | \
sed -re 's/(.*)/"\1"/' | jq -ra fromjson
# cat <(echo $together_json)
