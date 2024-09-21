.DEFAULT_GOAL := debug
.PHONY: setup setup-debug release debug paper clean

setup: install-libs
	rm -rf build* || true
	rm current_zone || true
	meson setup --native-file meson.ini build-rel --buildtype=release -Db_sanitize=none
	meson setup --native-file meson.ini build-dbg --buildtype=debug
	ln -s build-dbg build
	ln -s ~/.current_zone current_zone
	cd build && meson compile 

debug: #setup
	# cd build-dbg; meson compile
	make zstore

lib:
	mkdir lib

civetweb-v1.16.tar.gz:
	wget https://github.com/civetweb/civetweb/archive/v1.16.tar.gz
	mv v1.16.tar.gz $@

civetweb-1.16: civetweb-v1.16.tar.gz
	tar xfz civetweb-v1.16.tar.gz

lib/libcivetweb.a: lib civetweb-1.16
	cd civetweb-1.16 && \
		make clean lib PREFIX=$(PWD) COPT=-DNO_SSL WITH_CPP=1
	mv civetweb-1.16/libcivetweb.a lib

install-libs: lib/libcivetweb.a

# paper:
# 	@$(MAKE) -C atc2024

clean:
	# cd build-rel; meson compile --clean
	# cd build-dbg; meson compile --clean
	# cd builddir; meson compile --clean
	rm rw_test || true
	rm bdev_test || true
	rm nvme_test || true
	rm zstore || true

install-deps:
	pip3 install meson pyelftools
	sudo apt install -y pkg-config uuid-dev libfmt-dev libarchive-dev python3-pyelftools
	# libfmt-dev libaio-dev librados-dev mold \
	# sudo apt install -y meson libfmt-dev libaio-dev librados-dev mold \
	# 	libtcmalloc-minimal4 libboost-dev libradospp-dev \
	# 	liburing-dev 
	# sudo apt install -y libfuse3-dev

# makefile
CC := g++
PKG_CONFIG_PATH = /home/shwsun/dev/zstore/subprojects/spdk/build/lib/pkgconfig
ALL_SPDK_LIBS := spdk_accel_ioat spdk_blobfs spdk_jsonrpc \
spdk_accel_modules     spdk_blob           spdk_log \
spdk_accel             spdk_conf           spdk_lvol \
spdk_bdev_aio          spdk_dpdklibs       spdk_nbd \
spdk_bdev_delay        spdk_env_dpdk        \
spdk_bdev_error        spdk_env_dpdk_rpc   spdk_notify \
spdk_bdev_ftl          spdk_event_accel    spdk_nvme \
spdk_bdev_gpt          spdk_event_bdev     spdk_nvmf \
spdk_bdev_lvol         spdk_event_iscsi    spdk_rpc \
spdk_bdev_malloc       spdk_event_nbd      spdk_scsi \
spdk_bdev_modules          spdk_sock_modules \
spdk_bdev_null         spdk_event_nvmf     spdk_sock \
spdk_bdev_nvme         spdk_event          spdk_sock_posix \
spdk_bdev_passthru     spdk_event_scsi     spdk_syslibs \
spdk_bdev              spdk_event_sock     spdk_thread \
spdk_bdev_raid            spdk_trace \
spdk_bdev_split        spdk_event_vmd      spdk_util \
spdk_bdev_virtio       spdk_ftl            \
spdk_bdev_zone_block   spdk_ioat           spdk_vhost \
spdk_blob_bdev         spdk_iscsi          spdk_virtio \
spdk_blobfs_bdev       spdk_json           spdk_vmd	\
spdk_thread  spdk_nvme_tcp
SPDK_LIB := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config --cflags  --libs  $(ALL_SPDK_LIBS))


rw_test: src/rw_test.cc
	$(CC) src/rw_test.cc -pthread -g  -o rw_test  -Wl,--no-as-needed  $(SPDK_LIB)  -Wl,--as-needed

bdev_test: src/zns_bdev_test.cc
	$(CC) src/zns_bdev_test.cc -ggdb -pthread -g  -o bdev_test  -Wl,--no-as-needed  $(SPDK_LIB)  -Wl,--as-needed

nvme_test: src/zns_nvme_test.cc
	$(CC) src/zns_nvme_test.cc -ggdb -pthread -g  -o nvme_test  -Wl,--no-as-needed  $(SPDK_LIB)  -Wl,--as-needed

zstore: src/zns_bdev_test.cc
	$(CC) src/main.cc -pthread -g  -o zstore -Wl,--no-as-needed  $(SPDK_LIB)  -Wl,--as-needed
