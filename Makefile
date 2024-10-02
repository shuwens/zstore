.DEFAULT_GOAL := debug
.PHONY: setup setup-debug release debug paper clean

setup:
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

clean:
	cd build-rel; meson compile --clean
	cd build-dbg; meson compile --clean

install-deps:
	pip3 install meson pyelftools
	sudo apt install -y pkg-config uuid-dev libfmt-dev libarchive-dev python3-pyelftools
	# libfmt-dev libaio-dev librados-dev mold \
	# sudo apt install -y meson libfmt-dev libaio-dev librados-dev mold \
	# 	libtcmalloc-minimal4 libboost-dev libradospp-dev \
	# 	liburing-dev
	# sudo apt install -y libfuse3-dev
