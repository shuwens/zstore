#!/usr/bin/env bash

set -xeuo pipefail

s3bench -accessKey=KEY -accessSecret=SECRET \
	-bucket=db -operations=read \
	-skipCleanup -skipBucketCreate \
	-numClients=1000 -numSamples=100000  \
	-objectSize=23 -endpoint=http://127.0.0.1:2000
