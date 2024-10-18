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
		# 'b_sanitize=address,undefined',
		# 'b_asneeded=false',
		# 'b_lto=false',
		# 'b_lundef=false',
	],
)
fs = import('fs')
cc = meson.get_compiler('c')

add_project_arguments('-fPIC', language: 'cpp')
add_project_arguments('-Wno-unused-parameter', language: 'cpp')

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
	dependency(
		'boost',
		modules: ['system', 'filesystem', 'program_options', 'thread', 'regex'],
	),
]

# FIXME: there should be a better of doing this, but I don't know it yet.
custom_libnames = [
	'spdk_env_dpdk_rpc',
	'spdk_notify',
	'spdk_lvol',
	'spdk_blob',
	'spdk_nvme',
	'spdk_nvmf',
	'spdk_ftl',
	'spdk_virtio',
	'spdk_vfio_user',
	'spdk_accel',
	'spdk_dma',
	'spdk_accel_ioat',
	'spdk_ioat',
	'spdk_vmd',
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
	'spdk_conf',
	'spdk_lvol',
	'spdk_nbd',
	'spdk_scsi',
	'spdk_bdev_nvme',
	'spdk_vhost',
	'spdk_iscsi',
	'spdk_event_accel',
	'spdk_event_bdev',
	'spdk_event_iobuf',
	'spdk_event_iscsi',
	'spdk_event_keyring',
	'spdk_event_nbd',
	'spdk_event_nvmf',
	'spdk_event_scheduler',
	'spdk_event_scsi',
	'spdk_event',
	'spdk_event_sock',
	'spdk_event_vhost_blk',
	'spdk_event_vhost_scsi',
	'spdk_event_vmd',
	# 'spdk_rdma',
	# dpdk stuff
	'rte_bus_pci',
	'rte_compressdev',
	'rte_eal',
	'rte_kvargs',
	'rte_mempool_ring',
	'rte_net',
	'rte_rcu',
	'rte_security',
	'rte_vhost',
	'rte_bus_vdev',
	'rte_cryptodev',
	'rte_ethdev',
	'rte_log',
	'rte_mempool',
	'rte_pci',
	'rte_reorder',
	'rte_telemetry',
	'rte_cmdline',
	'rte_dmadev',
	'rte_hash',
	'rte_mbuf',
	'rte_meter',
	'rte_power',
	'rte_ring',
	'rte_timer',
	'spdk_env_dpdk_rpc',
	'spdk_env_dpdk',
	'spdk_scheduler_dpdk_governor',
	# bdev which we will remove
	'spdk_bdev',
	'spdk_bdev_aio',
	'spdk_bdev_ftl',
	'spdk_bdev_virtio',
	'spdk_event_bdev',
	'spdk_bdev',
	'spdk_bdev_malloc',
	'spdk_bdev_null',
	'spdk_bdev_nvme',
	'spdk_bdev_ftl',
	'spdk_bdev_passthru',
	'spdk_bdev_lvol',
	'spdk_bdev_raid',
	'spdk_bdev_error',
	'spdk_bdev_gpt',
	'spdk_bdev_split',
	'spdk_bdev_delay',
	'spdk_bdev_zone_block',
	'spdk_blobfs_bdev',
	'spdk_blobfs',
	'spdk_blob_bdev',
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

executable(
	'zstore',
	files('src/main.cc'),
	# dependencies: [zstore_deps, libs] + [dependency('_spdk')],
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	# include_directories: incdir,
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
# executable(
# 	'mpsd_read',
# 	files('examples/multipath/mp_one_drive_read.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )
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
executable(
	'http_async',
	files('examples/boost/http_server_async.cpp'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)
executable(
	'http_small',
	files('examples/boost/http_server_small.cpp'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)
executable(
	'http_awaitable',
	files('examples/boost/http_server_awaitable.cc'),
	dependencies: [zstore_deps] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)

# Tests
# -----------
# executable(
# 	'zns_test',
# 	files('src/tests/device_test.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )

# executable(
# 	'object_test',
# 	files('src/tests/object_test.cc'),
# 	dependencies: [zstore_deps] + [dependency('_spdk')],
# 	link_args: spdk_link_args,
# 	install: true,
# )
