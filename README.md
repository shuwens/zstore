# zstore

## Get started
```bash
set -x SPDK_DIR ..../SPDK
sudo ./zstore 1
sudo gdb --args ./zstore 1
```


TODO: Configuring the Linux NVMe over Fabrics Host



## Proof of concept design

remote is server `gundyr` and gateway is `pjdrz`.

- the ZNS SSD is installed on remote and exposed as a nvme-tgt, with nvme over
  tcp.
- 


## Store on NVME
https://news.ycombinator.com/item?id=37897921
