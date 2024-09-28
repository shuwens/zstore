#!/usr/bin/env bash

set -xeuo pipefail

s3bench -accessKey=KEY -accessSecret=SECRET \
	-bucket=db -endpoint=http://localhost:2000 \
	-objectSize=131 -operations=read \
	-skipCleanup -cpuprofile \
	-numClients=100 -numSamples=1000000 
