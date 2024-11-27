#!/bin/bash

bucket='test'
file_path='foo'
resource="/${bucket}/${file_path}"
# set url time to expire

expires=$(date +%s -d '4000 seconds')
stringtoSign="GET\n\n\n${expires}\n${resource}"
s3Key='minioadmin'
s3Secret='redbull64'
signature=`echo -en ${stringtoSign} | openssl sha1 -hmac ${s3Secret} -binary | base64`


curl -G http://localhost:9000/${file_path} \
    --data AWSAccessKeyId=${s3Key} \
    --data Expires=${expires}
    # --data-urlencode Signature=${signature}

