#!/bin/bash
# https://stackoverflow.com/questions/44751574/uploading-to-amazon-s3-via-curl-route

file=$1

bucket=test
resource="/${bucket}/${file}"
contentType="application/x-compressed-tar"
dateValue=`date -R`
stringToSign="PUT\n\n${contentType}\n${dateValue}\n${resource}"
s3Key=minioadmin
s3Secret=redbull64

signature=`echo -en ${stringToSign} | openssl sha1 -hmac ${s3Secret} -binary | base64`
curl -X PUT -T "${file}" \
  -H "Host: localhost:9000" \
  -H "Date: ${dateValue}" \
  -H "Content-Type: ${contentType}" \
  -H "Authorization: AWS ${s3Key}:${signature}" \
    http://localhost:9000/${file}
  # https://${bucket}.s3.amazonaws.com/${file}
