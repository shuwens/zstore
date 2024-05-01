project(
	'lsvd-rbd',
	'cpp',
	version: '0.1',
	default_options: [
		'cpp_std=c++20',
		'warning_level=2',
		'b_colorout=always',
	],
)

add_project_arguments('-fPIC', language: 'cpp')
add_project_arguments('-Wno-unused-parameter', language: 'cpp')

if get_option('buildtype') == 'debug'
	add_project_arguments('-fno-inline', language: 'cpp')
	add_project_arguments('-DLOGLV=1', language: 'cpp')

	# # add sanitizers for debug builds
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
	# szd_proj.dependency('szd_extended'),
	dependency('uuid'),
	dependency('numa'),
	dependency('openssl'),
	dependency('fmt'),
	dependency('xnvme'),
]

zstore_src = files(
	'src/main.cc',
)
executable('zstore', zstore_src, dependencies: zstore_deps)

# trying to use simpleznsdevice but failing

# cmake = import('cmake')
# szd_proj = cmake.subproject('simpleznsdevice')

# szd_deps = [
# 	szd_proj.dependency('szd_extended'),
# 	dependency('uuid'),
# 	dependency('numa'),
# 	dependency('openssl'),
# 	dependency('fmt'),
# 	dependency('spdk_nvme'),
# ]

# zstore_s_src = files(
# 	'src/main_szd.cc',
# )
# executable('zstore_s', zstore_s_src, dependencies: szd_deps)
