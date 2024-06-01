.DEFAULT_GOAL := debug
.PHONY: setup setup-debug release debug paper clean

setup:
	meson setup --native-file meson.ini build-rel --buildtype=release
	meson setup --native-file meson.ini build-dbg --buildtype=debug
	ln -s build-dbg build

clean:
	cd build-rel; meson compile --clean
	cd build-dbg; meson compile --clean

debug: setup
	cd build-dbg; meson compile

install-deps:
	# useful tools
	sudo apt install -y meson cmake nvme-cli smartmontools # numa
	# Folly deps
	sudo apt install libboost-all-dev libdouble-conversion-dev libevent-dev \
		libgflags-dev libgmock-dev libgoogle-glog-dev libgtest-dev \
		liblz4-dev liblzma-dev libsnappy-dev libsodium-dev libunwind-dev \
		libzstd-dev ninja-build zlib1g-dev
	# SPDK deps
	sudo apt install libnuma-dev libarchive-dev libibverbs-dev librdmacm-dev \
		python3-pyelftools libcunit1-dev libaio-dev
	# Zstore deps
	sudo apt install -y mold libfmt-dev libfuse3-dev net-tools \
		libjemalloc-dev liburing-dev pkg-config uuid-dev

install-spdk:
	sudo mv /usr/lib/python3.12/EXTERNALLY-MANAGED /usr/lib/python3.12/EXTERNALLY-MANAGED.bak
	cd subprojects; git clone https://github.com/spdk/spdk.git
	cd subprojects/spdk; sudo ./scripts/pkgdep.sh --all \
		git submodule update --init \
		./configure --with-rdma \
		make

install-xnvme:
	cd subprojects; git clone https://github.com/OpenMPDK/xNVMe.git xnvme
	cd subprojects/xnvme; git checkout next \
		sudo ./toolbox/pkgs/ubuntu-focal.sh \
		make build \
		sudo make install
