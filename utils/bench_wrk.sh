#!/usr/bin/env bash

set -xeuo pipefail


# ~/tools/wrk/wrk -t8 -c200 -d10s http://127.0.0.1:2000/db/bar
# ~/tools/wrk/wrk -t8 -c200 -d10s -s wrk-scripts/multiplepaths.lua http://127.0.0.1:2000/db/bar

# nohup wrk -t4 -c16 -d6h -H "X-Vault-Token: $VAULT_TOKEN" -s read-secrets.lua http://<vault_url>:8200 -- 1000 false > prod-test-read-1000-random-secrets-t4-c16-6hours.log &

# ~/tools/wrk/wrk -t8 -c200 -d10s -s random-reads.lua http://127.0.0.1:2000 -- 1000 false

~/tools/wrk/wrk -t8 -c800 -d10s -s random-reads.lua http://127.0.0.1:2000 -- 100000 false
