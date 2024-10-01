#!/usr/bin/env bash

set -xeuo pipefail

s3bench -accessKey=KEY -accessSecret=SECRET \
	-bucket=db -operations=read \
	-skipCleanup -skipBucketCreate \
	-numClients=10000 -numSamples=1000000  \
	-objectSize=23 -endpoint=http://127.0.0.1:2000
