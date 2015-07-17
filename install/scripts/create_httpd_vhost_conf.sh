#!/bin/bash
#
# $VUFIND_HOME and $VUFIND_LOCAL_DIR have to be set!
#
# Substitutes placeholders of templates/httpd-vufind-vhosts.conf
# and copies the file to the right location.
#
set -o errexit -o nounset
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
TEMPLATE_DIR=$SCRIPT_DIR/templates
TPL=$TEMPLATE_DIR/httpd-vufind-vhosts.conf
OUTPUT=$VUFIND_LOCAL_DIR/httpd-vufind-vhosts.conf

show_help() {
  cat << EOF
Substitutes placeholders of templates/httpd-vufind-vhosts.conf and copies the file to the right location.

USAGE: ${0##*/} SERVER_IP SERVER_URL [SSL_CERT SSL_KEY [SSL_CHAIN_FILE]]

SERVER_IP           The ip of this server (for apache/httpd)
SERVER_URL          The url of this server (for apache/httpd)

SSL_CERT            Path to the ssl certificate
SSL_KEY             Path to the key file of the ssl certificate

EOF
}

if [ "$#" -ne 2 ] && [ "$#" -ne 4 ] && [ "$#" -ne 5 ] ; then
  echo "ERROR: Illegal number of parameters!"
  echo "Parameters: $*"
  show_help
  exit 1
fi

SERVER_IP="$1"
SERVER_URL="$2"

USE_SSL=false
SSL_CERT=""
SSL_KEY=""
SSL_CHAIN_FILE="" 

if [ "$#" -eq 4 ] || [ "$#" -eq 5 ] ; then
  if [[ $3 ]] || [[ $4 ]]; then
    USE_SSL=true
  fi
  SSL_CERT="$3"
  SSL_KEY="$4"
fi
if [ "$#" -eq 5 ] ; then 
  SSL_CHAIN_FILE = "$5"
fi

TPL_CONTENT="$(cat $TPL)"

# Include SSL
PLACE="$(echo "$TPL_CONTENT" | awk 'match($0, /{{{if ssl [A-Za-z0-9\.\-]*[\s]*}}}/) { print $0 }')"
if [[ "$PLACE" ]] && [[ "$USE_SSL" = true ]]; then
  INC_TPL="$TEMPLATE_DIR/$(echo "$PLACE" | sed 's/{{{if ssl \([A-Za-z0-9\.\-]*\)}}}/\1/g')"
	INC_TPL_CONTENT="$(cat $INC_TPL)"
	TPL_CONTENT="$(echo "$TPL_CONTENT" | awk -v r="$INC_TPL_CONTENT" "{gsub(/$PLACE/,r)}1")"
elif [[ "$PLACE" ]] ; then
	TPL_CONTENT="$(echo "$TPL_CONTENT" | sed "s|$PLACE| |g")"
fi

# Include SSL_CHAIN_FILE
PLACE="$(echo "$TPL_CONTENT" | awk 'match($0, /{{{if sslChainFile [A-Za-z0-9\.\-]*[\s]*}}}/) { print $0 }')"
if [[ "$PLACE" ]] && [[ "$SSL_CHAIN_FILE" ]]; then
  INC_TPL="$TEMPLATE_DIR/$(echo $PLACE | sed 's/{{{if sslChainFile \([A-Za-z0-9\.\-]*\)}}}/\1/g')"
  INC_TPL_CONTENT="$(cat $INC_TPL)"
  TPL_CONTENT="$(echo "$TPL_CONTENT" | awk -v r="$INC_TPL_CONTENT" "{gsub(/$PLACE/,r)}1")"
elif [[ "$PLACE" ]] ; then
  TPL_CONTENT="$(echo "$TPL_CONTENT" | sed "s|$PLACE| |g")"
fi


TMP="$(echo "$TPL_CONTENT" | sed -e "s|{{{VUFIND_HOME}}}|$VUFIND_HOME|g" \
                                 -e "s|{{{VUFIND_LOCAL_DIR}}}|$VUFIND_LOCAL_DIR|g" \
                                 -e "s|{{{IP}}}|$SERVER_IP|g" \
                                 -e "s|{{{SERVER_URL}}}|$SERVER_URL|g" \
                                 -e "s|{{{SSL_CERT}}}|$SSL_CERT|g" \
                                 -e "s|{{{SSL_KEY}}}|$SSL_KEY|g" \
                                 -e "s|{{{SSL_CHAIN_FILE}}}|$SSL_CHAIN_FILE|g")"

echo "$TMP" > $OUTPUT
