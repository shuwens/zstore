.DEFAULT_GOAL := debug
.PHONY: setup setup-debug release debug paper clean

setup:
	rm -rf build* || true
	meson setup --native-file meson.ini build-rel --buildtype=release -Db_sanitize=none
	meson setup --native-file meson.ini build-dbg --buildtype=debug
	ln -s build-dbg build
	cd build && meson compile

debug: #setup
	# cd build-dbg; meson compile
	make zstore

clean:
	cd build-rel; meson compile --clean
	cd build-dbg; meson compile --clean

install-boost:
	sudo apt-get update -y
	sudo apt-get install build-essential g++ autotools-dev libicu-dev libbz2-dev libboost-all-dev -y   python-dev-is-python3
	# mkdir lib
	wget -O boost_1_86_0.tar.bz2 https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.bz2
	tar --bzip2 -xf boost_1_86_0.tar.bz2
	cd boost_1_86_0 && ./bootstrap.sh --prefix=/usr/
	cd boost_1_86_0 && ./b2
	cd boost_1_86_0 && sudo ./b2 install
	cat /usr/include/boost/version.hpp | grep "define BOOST_LIB_VERSION"


install-deps:
	pip3 install meson pyelftools
	sudo apt install -y pkg-config uuid-dev libfmt-dev libarchive-dev python3-pyelftools
	# libfmt-dev libaio-dev librados-dev mold \
	# sudo apt install -y meson libfmt-dev libaio-dev librados-dev mold \
	# 	libtcmalloc-minimal4 libboost-dev libradospp-dev \
	# 	liburing-dev
	# sudo apt install -y libfuse3-dev
