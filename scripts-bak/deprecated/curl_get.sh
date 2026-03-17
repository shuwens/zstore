#!/bin/sh
file=path/to/file
bucket=bucket-test
resource="/${bucket}/${file}"
contentType="application/x-compressed-tar"
dateValue="`date +'%a, %d %b %Y %H:%M:%S %z'`"
stringToSign="GET
${contentType}
${dateValue}
${resource}"
s3Key=50I9XIIDPA2TC8LKYUKM
s3Secret=J10qS4Bb4vIWxyJW3gWR4XATae0AbhRovkiiTOk
signature=`/bin/echo -en "$stringToSign" | openssl sha1 -hmac ${s3Secret} -binary | base64`
curl -H "Host: ${bucket}.12.12.12.2" \
-H "Date: ${dateValue}" \
-H "Content-Type: ${contentType}" \
-H "Authorization: AWS ${s3Key}:${signature}" \
http://${bucket}.12.12.12.2/${file}

