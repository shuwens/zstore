#!/usr/bin/env bash
set -euo pipefail

RED='\033[1;35m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

ssh zstore1 "cd ~/tools/apache-zookeeper-3.9.3-bin && ./bin/zkServer.sh start conf/zoo.cfg &"

sleep 1

echo -e "${GREEN}Starting ${RED}run.py${GREEN}...${NC}"

for node in zstore1 zstore3 zstore4 zstore5
do
	echo -e $node "start...."
	ssh ${node} "~/dev/zstore/utils/run_gateway.sh $node & " &
done
sleep 4

ssh zstore6 'cd ~/dev/zstore/utils && ./bench_read.sh &'

sleep 10

for node in zstore1 zstore3 zstore4 zstore5 zstore6
do
	ssh ${node} "sudo killall -r 'zstore' || true"
	ssh ${node} "cd ~/dev/zstore/ && git pull"
done

echo "Done"
