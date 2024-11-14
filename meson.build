project(
	'zstore',
	'cpp',
	'c',
	version: '0.1',
	default_options: [
		'cpp_std=c++23',
		'default_library=static',
		'warning_level=2',
		'b_colorout=always',
		# 'b_sanitize=address,undefined',
		# 'b_sanitize=thread',
		'b_lto=true',
		'b_thinlto_cache=true',
		# 'b_asneeded=false',
		# 'b_lto=false',
		# 'b_lundef=false',
	],
)
fs = import('fs')
cc = meson.get_compiler('c')

add_project_arguments(
	'-Wuninitialized',
	# '-DBOOST_ASIO_HAS_IO_URING',
	# '-DBOOST_ASIO_DISABLE_EPOLL',
	'-fPIC',
	'-Wno-unused-parameter',
	'-Wno-deprecated-declarations',
	'-fsized-deallocation',
	language: 'cpp',
)

if get_option('buildtype') == 'release'
	add_project_arguments('-g', language: 'c')
endif

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
	dependency('_spdk'),
	dependency('liburing', version: '>=2.0', include_type: 'system'),
	dependency(
		'boost',
		version: '>=1.86.0',
		modules: ['system', 'filesystem', 'program_options', 'thread', 'regex', 'timer'],
	),
]

# FIXME: there should be a better of doing this, but I don't know it yet.
custom_libnames = [
	'spdk_env_dpdk_rpc',
	'spdk_notify',
	'spdk_nvme',
	'spdk_accel',
	'spdk_sock',
	'spdk_sock_posix',
	'spdk_init',
	'spdk_thread',
	'spdk_trace',
	'spdk_rpc',
	'spdk_jsonrpc',
	'spdk_json',
	'spdk_env_dpdk',
	'spdk_util',
	'spdk_log',
	'spdk_event',
	'spdk_rdma',
	# dpdk stuff
	'rte_eal',
	'rte_kvargs',
	'rte_mempool_ring',
	'rte_net',
	'rte_vhost',
	'rte_mempool',
	'rte_pci',
	'rte_mbuf',
	'rte_ring',
	'spdk_env_dpdk_rpc',
	'spdk_env_dpdk',
	'spdk_bdev',
	'spdk_scsi',
]

spdk_paths = []
foreach libname : custom_libnames
	csd = meson.current_source_dir() / 'subprojects' / 'spdk'
	lib_dep = cc.find_library(
		libname,
		dirs: [
			csd / 'build' / 'lib',
			csd / 'dpdk' / 'build' / 'lib',
		],
	)
	paths = [
		csd / 'build' / 'lib' / 'lib' + libname + '.so',
		csd / 'dpdk' / 'build' / 'lib' / 'lib' + libname + '.so',
	]
	foreach path : paths
		if lib_dep.found() and fs.exists(path)
			spdk_paths += path
		endif
	endforeach
endforeach

# https://spdk.io/doc/pkgconfig.html
spdk_link_args = ['-Wl,--no-as-needed'] + spdk_paths + ['-Wl,--as-needed']

zstore_src = [
	'src/common.cc',
	'src/device.cc',
	'src/object.cc',
	'src/zstore_controller.cc',
	'src/main.cc',
	'src/tinyxml2.cpp',
	'src/zone.cc',
]

executable(
	'zstore',
	zstore_src,
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)

# Tests
# -----------
executable(
	'test_simple_object',
	files('src/tests/simple_object_test.cc'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)
executable(
	'test_different_objects',
	files('src/tests/different_objects_test.cc'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)

# FIXME
# reads from the drive
# executable(
# 	'spdk_read',
# 	files('examples/spdk_append.cc'),
# 	dependencies: [zstore_deps, libs] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )

# nvmf multipath signle drive
# executable(
# 	'mpsd_append',
# 	files('examples/multipath/mp_one_drive_append.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )
executable(
	'mpsd_read',
	files('examples/multipath/mp_one_drive_read.cc'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)
executable(
	'two_targets_read',
	files('examples/multipath/two_targets_read.cc'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)
# executable(
# 	'check_read',
# 	files('src/examples/check_read.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )

# nvmf arbitration
# executable(
# 	'test_read',
# 	files('examples/arbitration/arbitration_read.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )
# executable(
# 	'test_append',
# 	files('examples/arbitration/arbitration_append.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )

# localand rdma
# executable(
# 	'arbitration',
# 	files('examples/arbitration/arbitration.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )
# executable(
# 	'rdma_read',
# 	files('examples/arbitration/arbitration_rdma_read.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )
# executable(
# 	'rdma_append',
# 	files('examples/arbitration/arbitration_rdma_append.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )

# HTTP server
# executable(
# 	'http_async',
# 	files('examples/boost/http_server_async.cpp'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )
executable(
	'http_awaitable',
	files('examples/boost/http_server_awaitable.cc'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)
