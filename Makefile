.DEFAULT_GOAL := debug
.PHONY: setup setup-debug release debug paper clean

setup:
	rm -rf build* || true
	meson setup --native-file meson.ini build-rel --buildtype=release -Db_sanitize=none
	meson setup --native-file meson.ini build-dbg --buildtype=debug
	ln -s build-dbg build
	cd build && meson compile
	cd build-dbg && meson compile


debug:
	meson setup --native-file meson.ini build-dbg --buildtype=debug
	meson compile -C build-dbg

release:
	meson setup --native-file meson.ini build-rel --buildtype=release -Db_sanitize=none
	meson compile -C build-rel

clean:
	# meson -C build-rel clean
	# meson -C build-dbg clean
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
	sudo apt install -y meson nvme-cli net-tools
	sudo apt install -y clang-18 lld-18 cmake
	sudo apt install -y pkg-config uuid-dev libfmt-dev libarchive-dev python3-pyelftools libssl-dev libisal-dev

install-python:
	sudo apt install -y python3-pip
	sudo mv /usr/lib/python3.12/EXTERNALLY-MANAGED /usr/lib/python3.12/EXTERNALLY-MANAGED.old
	pip3 install meson pyelftools
