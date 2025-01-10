#!/usr/bin/env bash
set -euo pipefail

RED='\033[1;35m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
NC='\033[0m'


for node in zstore1 zstore3 zstore4 zstore5 zstore6
do
	echo -e $node "git pull...."
	ssh ${node} "cd ~/dev/zstore/ && git pull"
done
sleep 10



echo -e "${GREEN}Starting ${RED}run.py${GREEN}...${NC}"

for node in zstore1 zstore3 zstore4 zstore5
do
	echo -e $node "start...."
	ssh ${node} "~/dev/zstore/utils/run_gateway.sh $node"
done
sleep 10

ssh zstore6 '~/dev/zstore/utils/run_bench.sh'

echo "Done"
