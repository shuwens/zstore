project(
	'zstore',
	'cpp',
	'c',
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
]
libs = declare_dependency(
	link_args: [
		meson.current_source_dir() + '/lib/libcivetweb.a',
	],
)
incdir = include_directories('civetweb-1.16/include')

zstore_src = files('src/main.cc')
executable(
	'zstore',
	zstore_src,
	dependencies: [zstore_deps, libs] + [dependency('_spdk')],
)

# FIXME: there should be a better of doing this, but I don't know it yet.
custom_libnames = [
	'spdk_event',
	'spdk_env_dpdk_rpc',
	'spdk_event_bdev',
	'spdk_bdev',
	'spdk_notify',
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
	'spdk_lvol',
	'spdk_blob',
	'spdk_nvme',
	'spdk_nvmf',
	'spdk_bdev_aio',
	'spdk_bdev_ftl',
	'spdk_ftl',
	'spdk_bdev_virtio',
	'spdk_virtio',
	'spdk_vfio_user',
	'spdk_event_accel',
	'spdk_accel',
	'spdk_dma',
	# 'spdk_accel_error',
	'spdk_accel_ioat',
	'spdk_ioat',
	'spdk_event_vmd',
	'spdk_vmd',
	'spdk_event_sock',
	'spdk_sock',
	'spdk_sock_posix',
	'spdk_event_iobuf',
	'spdk_init',
	'spdk_thread',
	'spdk_trace',
	'spdk_rpc',
	'spdk_jsonrpc',
	'spdk_json',
	'spdk_env_dpdk',
	'spdk_util',
	'spdk_log',
	'rte_eal',
	'rte_mempool',
	'rte_ring',
	'rte_mbuf',
	'rte_bus_pci',
	'rte_pci',
	'rte_mempool_ring',
	'rte_telemetry',
	'rte_kvargs',
	'rte_rcu',
	'rte_power',
	'rte_ethdev',
	'rte_vhost',
	'rte_net',
	'rte_dmadev',
	'rte_cryptodev',
	'rte_hash',
	# zns
	# 'spdk_accel_modules',
	'spdk_conf',
	'spdk_lvol',
	# 'spdk_dpdklibs',
	'spdk_nbd',
	'spdk_bdev_gpt',
	'spdk_event_iscsi',
	'spdk_event_nbd',
	'spdk_scsi',
	# 'spdk_bdev_modules',
	# 'spdk_sock_modules',
	'spdk_event_nvmf',
	'spdk_bdev_nvme',
	'spdk_event',
	'spdk_event_scsi',
	# 'spdk_syslibs',
	'spdk_vhost',
	'spdk_iscsi',
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

kvstore_src = files('src/kvstore.cc')

simple_src = files('src/simple_append.cc')
multi_src = files('src/multi_append.cc')

executable(
	'bdev_test',
	files('src/zns_bdev_test.cc'),
	dependencies: [zstore_deps, libs] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)

executable(
	'nvme_test',
	files('src/zns_nvme_test.cc'),
	dependencies: [zstore_deps, libs] + [dependency('_spdk')],
	link_args: spdk_link_args,
	install: true,
)
