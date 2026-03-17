#!/usr/bin/env bash
set -xeuo pipefail


if [ $1 == "zstore1" ]; then
	cd ~/dev/zstore/build; meson compile; sudo ./zstore 6 1
elif [ $1 == "zstore3" ]; then
	cd ~/dev/zstore/build; meson compile; sudo ./zstore 6 3
elif [ $1 == "zstore4" ]; then
	cd ~/dev/zstore/build; meson compile; sudo ./zstore 6 4
elif [ $1 == "zstore5" ]; then
	cd ~/dev/zstore/build; meson compile; sudo ./zstore 6 5
fi
#
