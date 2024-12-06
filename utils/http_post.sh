#!/usr/bin/env bash
set -xeuo pipefail


# key='dpdk.sh' AWSAccessKeyId='minioadmin' policy='eyJleHBpcmF0aW9uIjogIjIwMjQtMTEtMjdUMTg6Mjk6MTdaIiwgImNvbmRpdGlvbnMiOiBbeyJidWNrZXQiOiAidGVzdCJ9LCB7ImtleSI6ICJkcGRrLnNoIn1dfQ==' signature='4KfU+UksgByJVhD2CffVvO9HW40='
# -F key='dpdk.sh' -F AWSAccessKeyId='minioadmin' -F policy='eyJleHBpcmF0aW9uIjogIjIwMjQtMTEtMjdUMTg6Mjk6MTdaIiwgImNvbmRpdGlvbnMiOiBbeyJidWNrZXQiOiAidGVzdCJ9LCB7ImtleSI6ICJkcGRrLnNoIn1dfQ==' -F signature='4KfU+UksgByJVhD2CffVvO9HW40='
# https://test.s3.amazonaws.com/

# http --form -v \
# 	key='dpdk.sh' \
#   http://localhost:9000 \
# 	AWSAccessKeyId='minioadmin' expires='1732734657' \
# 	signature='92bcpGaoq%2FnLr8ZcFumBlzl%2BLBk%3D' 

http --form -v \
  http://12.12.12.1:2000 \
  key='dpdk.sh' 


