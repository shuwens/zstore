# zstore

An distributed, eventually-consistent KV store built on ZNS SSDs

This project builds the following parts after compilation:
- `measurement`: we measure the append time, write time, read time, and zone
  full detection time.
- `multipath`: we use the multipath support from spdk to understand the
  reordering caused by running zone append in parallel
- `zstore`: the distributed, eventually-consistent object store built on ZNS SSDs

