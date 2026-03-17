#!/usr/bin/env bash
set -xeuo pipefail


sudo -E MINIO_ROOT_USER="minioadmin" \
	-E MINIO_ROOT_PASSWORD="redbull64" \
	-E MINIO_SERVER_URL="http://localhost:9000" \
	/usr/local/bin/minio server /home/shwsun/data/minio_data

# mc alias set myS3 https://s3.{your-region-code}.amazonaws.com/endpoint ACCESS_KEY SECRET_KEY

# bash +o history
# mc alias set ALIAS HOSTNAME ACCESS_KEY SECRET_KEY
# bash -o history

mc alias set myS3 http://12.12.12.1:2000 foo 12345678
mc alias set local http://127.0.0.1:9000 minioadmin minioadmin
