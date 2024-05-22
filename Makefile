.DEFAULT_GOAL := debug
.PHONY: setup setup-debug release debug paper clean

setup:
	meson setup builddir
	# meson setup --native-file meson.ini build-rel --buildtype=release
	# meson setup --native-file meson.ini build-dbg --buildtype=debug
	# ln -s build-dbg builddir

build: setup
	cd builddir; meson compile

clean:
	# cd build-rel; meson compile --clean
	# cd build-dbg; meson compile --clean
	cd builddir; meson compile --clean

debug: setup
	cd builddir; meson compile
	# cd build-dbg; meson compile

# ===================================================

# paper:
# 	@$(MAKE) -C atc2024
#
# install-deps:
# 	# sudo apt install -y meson libfmt-dev libaio-dev librados-dev mold \
# 	# 	libtcmalloc-minimal4 libboost-dev libradospp-dev \
# 	# 	liburing-dev pkg-config uuid-dev
# 	sudo apt install -y meson libfuse3-dev
#
