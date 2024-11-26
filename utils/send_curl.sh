#!/usr/bin/env bash
set -xeuo pipefail

export AWS_ACCESS_KEY_ID=minioadmin
export AWS_SECRET_ACCESS_KEY=redbull64
# export AWS_ACCESS_KEY_ID=AKxxx
# export AWS_SECRET_ACCESS_KEY=zzzz

date="$(date -u '+%a, %e %b %Y %H:%M:%S +0000')"
method="GET"
md5=""
path="/test"
printf -v string_to_sign "%s\n%s\n\n%s\n%s" "$method" "$md5" "$date" "$path"
signature=$(echo -n "$string_to_sign" | openssl sha1 -binary -hmac "${AWS_SECRET_ACCESS_KEY}" | openssl base64)
authorization="AWS ${AWS_ACCESS_KEY_ID}:${signature}"

curl -o test -s -f -H Date:"${date}" -H Authorization:"${authorization}" http://localhost:9000"${path}"
