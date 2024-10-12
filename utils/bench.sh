#!/usr/bin/env bash

set -xeuo pipefail

# s3bench -accessKey=KEY -accessSecret=SECRET \
# 	-bucket=db -operations=read \
# 	-skipCleanup -skipBucketCreate \
# 	-numClients=100 -numSamples=100000 \
# 	-objectSize=23 -endpoint=http://127.0.0.1:2000

s3bench -accessKey=KEY -accessSecret=SECRET \
	-bucket=db -operations=write \
	-skipCleanup -skipBucketCreate \
	-numClients=100 -numSamples=1000 \
	-endpoint=http://127.0.0.1:2000

