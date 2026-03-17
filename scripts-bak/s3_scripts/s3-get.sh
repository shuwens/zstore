#!/usr/bin/env bash
#
# Usage:
#   s3-get.sh <bucket> <region> <source-file> <dest-path>
#
# Description:
#   Retrieve a secured file from S3 using AWS signature 4.
#   To run, this shell script depends on command-line curl and openssl
#
# References:
#   https://czak.pl/2015/09/15/s3-rest-api-with-curl.html
#   https://gist.github.com/adrianbartyczak/1a51c9fa2aae60d860ca0d70bbc686db
#

# set -x
set -e

script="${0##*/}"
usage="USAGE: $script <bucket> <region> <source-file> <dest-path>

Example: $script dev.build.artifacts us-east-1 /jobs/dev-job/1/dist.zip ./dist.zip"

[ $# -ne 4 ] && printf "ERROR: Not enough arguments passed.\n\n$usage\n" && exit 1

[ -z "$AWS_ACCESS_KEY_ID" -o -z "$AWS_SECRET_ACCESS_KEY" ] \
    && printf "ERROR: AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY environment variables must be defined.\n" && exit 1

[ ! type openssl 2>/dev/null ] && echo "openssl is required and must be installed" && exit 1
[ ! type curl 2>/dev/null ] && echo "curl is required and must be installed" && exit 1


AWS_SERVICE='s3'
AWS_REGION="$2"
# AWS_SERVICE_ENDPOINT_URL="${AWS_SERVICE}.${AWS_REGION}.amazonaws.com"
AWS_SERVICE_ENDPOINT_URL="http://localhost:9000"
AWS_S3_BUCKET_NAME="$1"
AWS_S3_PATH="$(echo $3 | sed 's;^\([^/]\);/\1;')"



# Create an SHA-256 hash in hexadecimal.
# Usage:
#   hash_sha256 <string>
function hash_sha256 {
  printf "${1}" | openssl dgst -sha256 | sed 's/^.* //'
}

# Create an SHA-256 hmac in hexadecimal.
# Usage:
#   hmac_sha256 <key> <data>
function hmac_sha256 {
  printf "${2}" | openssl dgst -sha256 -mac HMAC -macopt "${1}" | sed 's/^.* //'
}

CURRENT_DATE_DAY="$(date -u '+%Y%m%d')"
CURRENT_DATE_ISO8601="${CURRENT_DATE_DAY}T$(date -u '+%H%M%S')Z"

HTTP_REQUEST_PAYLOAD_HASH="$(printf "" | openssl dgst -sha256 | sed 's/^.* //')"
# HTTP_CANONICAL_REQUEST_URI="/${AWS_S3_BUCKET_NAME}${AWS_S3_PATH}"
HTTP_CANONICAL_REQUEST_URI="/${AWS_S3_BUCKET_NAME}"
HTTP_REQUEST_CONTENT_TYPE='application/octet-stream'

HTTP_CANONICAL_REQUEST_HEADERS="content-type:${HTTP_REQUEST_CONTENT_TYPE}
host:${AWS_SERVICE_ENDPOINT_URL}
x-amz-content-sha256:${HTTP_REQUEST_PAYLOAD_HASH}
x-amz-date:${CURRENT_DATE_ISO8601}"
# Note: The signed headers must match the canonical request headers.
HTTP_REQUEST_SIGNED_HEADERS="content-type;host;x-amz-content-sha256;x-amz-date"
HTTP_CANONICAL_REQUEST="GET
${HTTP_CANONICAL_REQUEST_URI}\n
${HTTP_CANONICAL_REQUEST_HEADERS}\n
${HTTP_REQUEST_SIGNED_HEADERS}
${HTTP_REQUEST_PAYLOAD_HASH}"

# Create the signature.
# Usage:
#   create_signature
function create_signature {
  stringToSign="AWS4-HMAC-SHA256\n${CURRENT_DATE_ISO8601}\n${CURRENT_DATE_DAY}/${AWS_REGION}/${AWS_SERVICE}/aws4_request\n$(hash_sha256 "${HTTP_CANONICAL_REQUEST}")"
  dateKey=$(hmac_sha256 key:"AWS4${AWS_SECRET_ACCESS_KEY}" "${CURRENT_DATE_DAY}")
  regionKey=$(hmac_sha256 hexkey:"${dateKey}" "${AWS_REGION}")
  serviceKey=$(hmac_sha256 hexkey:"${regionKey}" "${AWS_SERVICE}")
  signingKey=$(hmac_sha256 hexkey:"${serviceKey}" "aws4_request")

  printf "${stringToSign}" | openssl dgst -sha256 -mac HMAC -macopt hexkey:"${signingKey}" | sed 's/(stdin)= //'
}

SIGNATURE="$(create_signature)"
HTTP_REQUEST_AUTHORIZATION_HEADER="\
AWS4-HMAC-SHA256 Credential=${AWS_ACCESS_KEY_ID}/${CURRENT_DATE_DAY}/\
${AWS_REGION}/${AWS_SERVICE}/aws4_request, \
SignedHeaders=${HTTP_REQUEST_SIGNED_HEADERS}, Signature=${SIGNATURE}"

[ -d $4 ] && OUT_FILE="$4/$(basename $AWS_S3_PATH)" || OUT_FILE=$4
echo "Downloading ${AWS_SERVICE_ENDPOINT_URL}${HTTP_CANONICAL_REQUEST_URI} to $OUT_FILE"

curl "${AWS_SERVICE_ENDPOINT_URL}${HTTP_CANONICAL_REQUEST_URI}" \
    -H "Authorization: ${HTTP_REQUEST_AUTHORIZATION_HEADER}" \
    -H "content-type: ${HTTP_REQUEST_CONTENT_TYPE}" \
    -H "x-amz-content-sha256: ${HTTP_REQUEST_PAYLOAD_HASH}" \
    -H "x-amz-date: ${CURRENT_DATE_ISO8601}" \
    -f -S -o ${OUT_FILE}
