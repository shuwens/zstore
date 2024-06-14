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

lib:
	mkdir lib
#
# include:
# 	mkdir include

curlpp-0.8.1.tar.gz:
	wget https://github.com/jpbarrette/curlpp/archive/refs/tags/v0.8.1.tar.gz
	mv v0.8.1.tar.gz $@

lib/libcurlpp.a: curlpp-0.8.1.tar.gz
	tar -xzf curlpp-0.8.1.tar.gz
	cd curlpp-0.8.1/ ; \
	cmake . && make && sudo make install
	sudo ldconfig
	mv curlpp-0.8.1/libcurlpp.a lib/libcurlpp.a
	# cmake . ; \
	# cmake --build . --target install --config Debug --install-prefix=`pwd`/../curlpp/

civetweb-v1.12.tar.gz:
	wget https://github.com/civetweb/civetweb/archive/v1.12.tar.gz
	mv v1.12.tar.gz $@

civetweb-1.12: civetweb-v1.12.tar.gz
	tar xfz civetweb-v1.12.tar.gz

civetweb-v1.16.tar.gz:
	wget https://github.com/civetweb/civetweb/archive/v1.16.tar.gz
	mv v1.16.tar.gz $@

civetweb-1.16: civetweb-v1.16.tar.gz
	tar xfz civetweb-v1.16.tar.gz

lib/libcivetweb.a: civetweb-1.16 lib #include
	cd civetweb-1.16 && \
	make clean lib PREFIX=$(PWD) COPT=-DNO_SSL WITH_CPP=1
	mv civetweb-1.16/libcivetweb.a lib

install-libs: lib/libcurlpp.a lib/libcivetweb.a
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
	sudo apt install -y mold libfmt-dev libfuse3-dev net-tools libcurlpp-dev \
		libjemalloc-dev liburing-dev pkg-config uuid-dev libssl-dev httpie

install-spdk:
	sudo mv /usr/lib/python3.12/EXTERNALLY-MANAGED /usr/lib/python3.12/EXTERNALLY-MANAGED.bak  || true
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
