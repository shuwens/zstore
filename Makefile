.DEFAULT_GOAL := debug
.PHONY: setup setup-debug release debug paper clean

setup:
	meson setup --native-file meson.ini build-rel --buildtype=release
	meson setup --native-file meson.ini build-dbg --buildtype=debug
	ln -s build-dbg builddir

debug: setup
	cd build-dbg; meson compile

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

paper:
	@$(MAKE) -C atc2024

clean:
	# cd build-rel; meson compile --clean
	# cd build-dbg; meson compile --clean
	cd builddir; meson compile --clean
	rm rw_test

install-deps:
	# sudo apt install -y meson libfmt-dev libaio-dev librados-dev mold \
	# 	libtcmalloc-minimal4 libboost-dev libradospp-dev \
	# 	liburing-dev pkg-config uuid-dev
	sudo apt install -y meson libfuse3-dev
