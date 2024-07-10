project(
	'zstore',
	'cpp',
	version: '0.1',
	default_options: [
		'cpp_std=c++20',
		# 'cpp_std=c++23',
		'warning_level=2',
		'b_colorout=always',
		# 'b_sanitize=address,undefined',
		'b_lto=true',
		'b_thinlto_cache=true',
	],
)

add_project_arguments('-fPIC', language: 'cpp')
add_project_arguments('-Wno-unused-parameter', language: 'cpp')

if get_option('buildtype') == 'debug'
	add_project_arguments('-fno-inline', language: 'cpp')
	add_project_arguments('-DLOGLV=1', language: 'cpp')
	# add sanitizers for debug builds
	# add_project_arguments(
	# 	'-fsanitize=address,undefined,nullability,implicit-conversion',
	# 	language: 'cpp',
	# )
	# add_project_link_arguments(
	# 	'-fsanitize=address,undefined',
	# 	'-Wl,--unresolved-symbols=ignore-in-object-files',
	# 	language: 'cpp',
	# )
endif

zstore_deps = [
	dependency('uuid'),
	dependency('numa'),
	dependency('openssl'),
	dependency('fmt'),
	dependency('xnvme'),
]
libs = declare_dependency(
	link_args: [
		meson.current_source_dir() + '/lib/libcivetweb.a',
	],
)
incdir = include_directories('civetweb-1.16/include')

zstore_src = files('src/main.cc')
kvstore_src = files('src/kvstore.cc')
simple_src = files('src/simple_append.cc')
multi_src = files('src/multi_append.cc')
rw_src = files('src/zns_bdev_test.cc')

# executable(
# 	'kvstore',
# 	kvstore_src,
# 	dependencies: [zstore_deps, libs],
# 	include_directories: incdir,
# )
# executable('simple', simple_src, dependencies: [zstore_deps])
# executable('multi', multi_src, dependencies: [zstore_deps])
# executable(
# 	'zstore',
# 	zstore_src,
# 	dependencies: [zstore_deps, libs],
# 	include_directories: incdir,
# )

# local_env = environment()
#
# test_env = environment()
# test_env.append('LD_LIBRARY_PATH',
#     meson.project_build_root() / 'subprojects' / 'spdk'/'dpdk'/'build'/'lib',
#     meson.project_build_root() / 'subprojects' / 'spdk'/'build'/'lib')

# FIXME: This is currently broken
executable(
	'rw_test',
	rw_src,
	build_rpath: '/home/shwsun/dev/zstore/subprojects/spdk/dpdk/build/lib:/home/shwsun/dev/zstore/subprojects/spdk/build/lib',
	dependencies: [zstore_deps, libs] + [dependency('_spdk')],
)
