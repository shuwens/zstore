project(
	'zstore',
	'cpp',
	'c',
	version: '0.1',
	default_options: [
		'cpp_std=c++23',
		'optimization=3',
		'default_library=static',
		'warning_level=2',
		'b_colorout=always',
		'b_sanitize=address,undefined',
		# 'b_sanitize=thread',
		'b_lto=true',
		'b_thinlto_cache=true',
		# 'b_asneeded=false',
		# 'b_lundef=false',
	],
)
fs = import('fs')
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

add_project_arguments(
	'-Wuninitialized',
	'-fPIC',
	'-Wno-unused-parameter',
	'-Wno-deprecated-declarations',
	'-fsized-deallocation',
	language: 'cpp',
)

if get_option('buildtype') == 'release'
	add_project_arguments('-DPERF=1', language: 'cpp')
endif

if get_option('buildtype') == 'debug'
	add_project_arguments('-fno-inline', language: 'cpp')
	add_project_arguments('-DLOGLV=1', language: 'cpp')
endif

zk_inc_dir = [
	include_directories(
		'/home/shwsun/tools/apache-zookeeper-3.9.3/zookeeper-client/zookeeper-client-c/include',
	),
]

zstore_deps = [
	dependency('uuid'),
	dependency('openssl'),
	dependency('fmt'),
	dependency('_spdk'),
	dependency('liburing', version: '>=2.0', include_type: 'system'),
	dependency(
		'boost',
		version: '>=1.86.0',
		modules: [
			'serialization',
			'system',
			'filesystem',
			'program_options',
			'thread',
			'regex',
			'timer',
		],
	),
]

rdma_libs = [
	cc.find_library('rdmacm'),
	cc.find_library('ibverbs'),
]
rdma = declare_dependency(dependencies: rdma_libs)

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
	'spdk_bdev_malloc',
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

# add zookeeper library
zookeeper_path = meson.current_source_dir() / '..' / '..' / '..' / '..' / 'usr' / 'local' / 'lib' / 'libzookeeper_mt.so'

spdk_paths += zookeeper_path

# https://spdk.io/doc/pkgconfig.html
spdk_link_args = ['-Wl,--no-as-needed'] + spdk_paths + ['-Wl,--as-needed']

zstore_src = [
	'src/common.cc',
	'src/device.cc',
	'src/object.cc',
	'src/http_server.cc',
	'src/zstore_controller.cc',
	'src/tinyxml2.cpp',
	'src/zone.cc',
	'src/rdma_common.cc',
	'src/endpoint.cpp',
	'src/magic_buffer.cpp',
]

executable(
	'zstore',
	zstore_src + ['src/main.cc'],
	dependencies: [zstore_deps] + [rdma],
	include_directories: zk_inc_dir,
	link_args: spdk_link_args,
	install: true,
)

# Tests
# -----------
t1 = executable(
	'test_simple_object',
	files('src/tests/simple_object_test.cc'),
	dependencies: [zstore_deps],
	link_args: spdk_link_args,
)
test('Simple object', t1)

t2 = executable(
	'test_different_objects',
	files('src/tests/different_objects_test.cc'),
	dependencies: [zstore_deps],
	link_args: spdk_link_args,
	install: true,
)
test('Different object', t2)

# TODO i think I want to rethink about this anyway

# t3 = executable(
# 	'unittest_map',
# 	files('src/tests/map_unittest.cc'),
# 	dependencies: [zstore_deps],
# 	link_args: spdk_link_args,
# 	install: true,
# )
# test('Map dump and load unit tests', t3)

t4 = executable(
	'test_4KB_object',
	files('src/tests/4KB_object_test.cc'),
	dependencies: [zstore_deps],
	link_args: spdk_link_args,
	install: true,
)
test('4KB object', t4)

t5 = executable(
	'test_4MB_object',
	files('src/tests/4MB_object_test.cc'),
	dependencies: [zstore_deps],
	link_args: spdk_link_args,
	install: true,
)
test('4MB object', t5)

# unit tests
t6 = executable(
	'test_append_read',
	zstore_src + ['src/tests/unit_tests/append_read_test.cc'],
	dependencies: [zstore_deps] + [rdma],
	include_directories: zk_inc_dir,
	link_args: spdk_link_args,
	install: true,
)
test('Append and read buffer', t6)

# executable(
# 	'test_map',
# 	files('src/tests/map_test.cc'),
# 	dependencies: [zstore_deps] ,
# 	link_args: spdk_link_args,
# 	install: true,
# )

# nvmf multipath signle drive
executable(
	'mpsd_append',
	files('examples/multipath/mp_one_drive_append.cc'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)
executable(
	'mpsd_read',
	files('examples/multipath/mp_one_drive_read.cc'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)
# executable(
# 	'test_append_read',
# 	files('examples/test_append_read.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )
executable(
	'three_targets_reads',
	files('examples/multipath/three_target_reads.cc'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)
executable(
	'three_targets_writes',
	files('examples/multipath/three_target_writes.cc'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)

# zookeeper
executable(
	'zkclient',
	files('examples/zookeeper/zkclient.cc'),
	dependencies: [zstore_deps],
	include_directories: zk_inc_dir,
	link_args: spdk_link_args,
	install: true,
)
executable(
	'zkleader',
	files('examples/zookeeper/zkleader.cc'),
	dependencies: [zstore_deps],
	include_directories: zk_inc_dir,
	link_args: spdk_link_args,
	install: true,
)
executable(
	'zklock',
	files('examples/zookeeper/zklock.cc'),
	dependencies: [zstore_deps],
	include_directories: zk_inc_dir,
	link_args: spdk_link_args,
	install: true,
)
executable(
	'zktest',
	files('examples/zookeeper/zktest.cc'),
	dependencies: [zstore_deps],
	include_directories: zk_inc_dir,
	link_args: spdk_link_args,
	install: true,
)

# rdma
executable(
	'circular_buffer_client',
	files('examples/rdma/circular_buffer_client.cc'),
	dependencies: [zstore_deps] + [rdma],
	link_args: spdk_link_args,
	install: true,
)
executable(
	'circular_buffer_server',
	files('examples/rdma/circular_buffer_server.cc'),
	dependencies: [zstore_deps] + [rdma],
	link_args: spdk_link_args,
	install: true,
)
