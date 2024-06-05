.DEFAULT_GOAL := debug
.PHONY: setup setup-debug release debug paper clean

setup: lib/libcivetweb.a
	meson setup --native-file meson.ini build-rel --buildtype=release
	meson setup --native-file meson.ini build-dbg --buildtype=debug
	ln -s build-dbg build

clean:
	cd build-rel; meson compile --clean
	cd build-dbg; meson compile --clean

debug:
	cd build-dbg; meson compile

# lib:
# 	mkdir lib
#
# include:
# 	mkdir include

# curlpp/lib/libcurlpp.a:
# 	tar -xzf curlpp-0.7.3.tar.gz
# 	cd curlpp-0.7.3/ ; \
# 	./configure --prefix=`pwd`/../curlpp/ ; \
# 	make ; make install

civetweb-v1.12.tar.gz:
	wget https://github.com/civetweb/civetweb/archive/v1.12.tar.gz
	mv v1.12.tar.gz $@

civetweb-1.12: civetweb-v1.12.tar.gz
	tar xfz civetweb-v1.12.tar.gz

lib/libcivetweb.a: civetweb-1.12 # lib include
	cd civetweb-1.12 && \
	make lib PREFIX=$(PWD) COPT=-DNO_SSL WITH_CPP=1

# install-libs: curlpp/lib/libcurlpp.a lib/libcivetweb.a
# install-libs: lib/libcivetweb.a

install-deps:
	# useful tools
	sudo apt install -y meson cmake nvme-cli curl smartmontools # numa
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
