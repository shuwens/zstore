# copied from xnvme's spdk subproject configuration and cleaned up a bit

project('spdk', 'c', version: '24.05')
if host_machine.system() != 'linux'
  error('Unsupported system type "@0@"'.format(exec_env))
endif

fs = import('fs')
cc = meson.get_compiler('c')

math_dep = cc.find_library('m', has_headers: ['math.h'])
ssl_dep = dependency('openssl', version: '>=1.1.1')
dlfcn_dep = cc.find_library('dl', has_headers: ['dlfcn.h'])
uuid_dep = cc.find_library('uuid', dirs: [], has_headers: ['uuid/uuid.h'])
numa_dep = cc.find_library('numa', has_headers: ['numaif.h'])
archive_dep = cc.find_library('archive', has_headers: ['archive.h'])

if get_option('build_subprojects') and not fs.exists('build')
  message('Configuring ..')
  if get_option('buildtype') == 'debug'
    run_command(['configure-spdk.sh', 'debug'], capture: true, check: true)
  else
    run_command(['configure-spdk.sh', 'release'], capture: true, check: true)
  endif
endif

if get_option('build_subprojects') and not fs.exists('build' / 'lib' / 'libspdk_nvme.a')
  message('Building ..')
  run_command([find_program('make'), '-j', '20'], capture: true, check: true, env: {})
endif

message('Setting up dependency ..')
message('build_subprojects:', get_option('build_subprojects'))

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
  'isal',
  'isal_crypto',
  'spdk_conf',
  'spdk_lvol',
  'spdk_nbd',
  'spdk_scsi',
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
  # 'spdk_rdma',
]

spdk_deps = [
  dlfcn_dep,
  math_dep,
  numa_dep,
  uuid_dep,
  ssl_dep,
  ssl_dep,
  archive_dep,
]

spdk_paths = []
foreach libname : custom_libnames
  csd = meson.current_source_dir()
  lib_dep = cc.find_library(
    libname,
    dirs: [
      csd / 'build' / 'lib',
      csd / 'dpdk' / 'build' / 'lib',
      csd / 'isa-l' / '.libs',
      csd / 'isa-l-crypto' / '.libs',
    ],
    # static: true,
  )

  # Create a bunch of paths
  paths = [
    csd / 'build' / 'lib' / 'lib' + libname + '.a',
    csd / 'dpdk' / 'build' / 'lib' / 'lib' + libname + '.a',
    csd / 'isa-l' / '.libs' / 'lib' + libname + '.a',
    csd / 'isa-l-crypto' / '.libs' / 'lib' + libname + '.a',
  ]
  foreach path : paths
    if lib_dep.found() and fs.exists(path)
      spdk_paths += path
    endif
  endforeach
endforeach

# devenv = environment()
# builddir = meson.project_build_root()
# srcdir = meson.project_source_root()
# devenv.set('LD_LIBRARY_PATH', meson.current_source_dir())
# devenv.append('LD_LIBRARY_PATH', meson.current_source_dir() / 'dpdk' / 'build' / 'lib')
# devenv.append('LD_LIBRARY_PATH', meson.current_source_dir() / 'build' / 'lib')
# meson.add_devenv(devenv)

spdk_inc = get_option('build_subprojects') ? include_directories('dpdk' / 'build' / 'include', 'build' / 'include') : include_directories('.')
# https://spdk.io/doc/pkgconfig.html
spdk_link_args = ['-Wl,--no-as-needed'] + spdk_paths + ['-Wl,--as-needed']

# Construct link_args based on the above
spdk_dep = declare_dependency(
  dependencies: spdk_deps,
  link_args: spdk_link_args,
  include_directories: spdk_inc,
)
