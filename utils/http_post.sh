
# key='dpdk.sh' AWSAccessKeyId='minioadmin' policy='eyJleHBpcmF0aW9uIjogIjIwMjQtMTEtMjdUMTg6Mjk6MTdaIiwgImNvbmRpdGlvbnMiOiBbeyJidWNrZXQiOiAidGVzdCJ9LCB7ImtleSI6ICJkcGRrLnNoIn1dfQ==' signature='4KfU+UksgByJVhD2CffVvO9HW40='
# -F key='dpdk.sh' -F AWSAccessKeyId='minioadmin' -F policy='eyJleHBpcmF0aW9uIjogIjIwMjQtMTEtMjdUMTg6Mjk6MTdaIiwgImNvbmRpdGlvbnMiOiBbeyJidWNrZXQiOiAidGVzdCJ9LCB7ImtleSI6ICJkcGRrLnNoIn1dfQ==' -F signature='4KfU+UksgByJVhD2CffVvO9HW40='
# https://test.s3.amazonaws.com/

http --form -v \
  http://localhost:9000 \
	key='dpdk.sh' AWSAccessKeyId='minioadmin' \
	signature='92bcpGaoq%2FnLr8ZcFumBlzl%2BLBk%3D' \
  expires='1732734657'
