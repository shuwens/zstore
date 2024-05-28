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

install-deps:
	# useful tools
	sudo apt install -y meson cmake # numa
	# useful tools
	sudo apt install -y nvme-cli smartmontools
	sudo apt install -y libfmt-dev libaio-dev librados-dev mold \
		libtcmalloc-minimal4 libboost-dev libradospp-dev \
		liburing-dev pkg-config uuid-dev libfuse3-dev

install-spdk:
	sudo mv /usr/lib/python3.12/EXTERNALLY-MANAGED /usr/lib/python3.12/EXTERNALLY-MANAGED.bak
	cd subprojects
	git clone https://github.com/spdk/spdk.git
	cd spdk
	sudo ./scripts/pkgdep.sh --all

install-xnvme:
	pushd subprojects
	git clone https://github.com/OpenMPDK/xNVMe.git xnvme
	cd xnvme
	git checkout next
	sudo ./toolbox/pkgs/ubuntu-focal.sh
	make build
	sudo make install
