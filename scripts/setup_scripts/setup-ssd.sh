#!/bin/bash
#
sudo mkdir -p /mnt/minio1
sudo mkdir -p /mnt/minio2

if [ "$(hostname)" == "zstore2" ]; then
	sudo mkfs.ext4 /dev/nvme1n1
	sudo mkfs.ext4 /dev/nvme4n1
	sudo mount /dev/nvme1n1 /mnt/minio1
	sudo mount /dev/nvme4n1 /mnt/minio2
elif [ "$(hostname)" == "zstore3" ]; then
	sudo mkfs.ext4 /dev/nvme0n1
	sudo mkfs.ext4 /dev/nvme3n1
	sudo mount /dev/nvme0n1 /mnt/minio1
	sudo mount /dev/nvme3n1 /mnt/minio2
elif [ "$(hostname)" == "zstore4" ]; then
	sudo mkfs.ext4 /dev/nvme0n1
	sudo mkfs.ext4 /dev/nvme4n1
	sudo mount /dev/nvme0n1 /mnt/minio1
	sudo mount /dev/nvme4n1 /mnt/minio2
else
	echo "on wrong server"
fi

sudo mkdir -p /mnt/minio1/minio
sudo mkdir -p /mnt/minio2/minio

sudo chown minio-user:minio-user /mnt/minio1 /mnt/minio2 

sudo -E MINIO_ROOT_USER="minioadmin" \
	-E MINIO_ROOT_PASSWORD="admin123" \
	-E MINIO_OPTS="--console-address :9001" \
    -E MINIO_SERVER_URL="http://minio.example.net:9000" \
    minio server  "http://12.12.12.{2...4}:9000/mnt/minio{1...2}/minio"
