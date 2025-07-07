#/bin/bash

if [ $# != 1 ]; then
    echo "Usage $0 derivation.txt"
    exit 1
fi

derivation_file="$1"
derivation_result=$(cat ${derivation_file} | \
                   awk '/<\/think>/{flag=1} flag' | \
                   jq --raw-input --slurp ) 
payload=$(cat <<EOF 
{
    "model": "deepseek-ai/DeepSeek-R1",
    "messages": [
      {
        "role": "system",
        "content": "You are an extractor for the final result of a derivation of a class assignment for bibliographic records. From the given text extract the class abbreviations as a json array. Do not output anything else"
      },
      {
        "role": "user",
        "content": ${derivation_result}
      }
    ],
    "stream": false
}
EOF
)

curl --silent -X POST "https://api.together.xyz/v1/chat/completions" \
  -H "Authorization: Bearer $TOGETHER_API_KEY" \
  -H "Content-Type: application/json" \
  -d @<(echo ${payload}) \
  | jq -r '.choices[0].message.content' | awk '/<\/think>/{flag=1} flag' \
  | sed '/<\/think>/d' | sed '/^```/d'
