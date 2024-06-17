project(
	'zstore',
	'c',
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
add_project_arguments('-fPIC', language: 'cpp')


if get_option('buildtype') == 'debug'
	add_project_arguments('-fno-inline', language: 'cpp')
	add_project_arguments('-DLOGLV=1', language: 'cpp')

	# # add sanitizers for debug builds
	# add_project_arguments(
	# 	'-fsanitize=address,undefined',
	# 	language: 'cpp',
	# )
	# add_project_link_arguments(
	# 	'-fsanitize=address,undefined',
	# 	'-Wl,--unresolved-symbols=ignore-in-object-files',
	# 	language: 'cpp',
	# )
elif get_option('buildtype') == 'release'
	add_project_arguments('-DLOGLV=2', language: 'cpp')
endif

cpp = meson.get_compiler('cpp')

zstore_deps = [
	# dependency('avl'),
	dependency('boost'),
	dependency('uuid'),
	dependency('numa'),
	dependency('openssl'),
	dependency('fmt'),
	dependency('xnvme'),
	cpp.find_library('curlpp'),
	cpp.find_library('curl'),
]

libs = declare_dependency(
  link_args : [
    meson.current_source_dir() + '/lib/libcivetweb.a',
    meson.current_source_dir() + '/lib/libcurlpp.a',
  ]
)

zstore_src = files(
	'src/main.cc',
)

incdir = include_directories('civetweb-1.16/include')

executable('zstore', zstore_src, dependencies: [libs, zstore_deps],  include_directories : incdir)
executable('simple_zstore', 'src/simple_append.cc', dependencies: [libs, zstore_deps],  include_directories : incdir)
executable('mc_zstore', 'src/mc_append.cc', dependencies: [libs, zstore_deps],  include_directories : incdir)
executable('zone', 'src/third_party/zoned.cc', dependencies: zstore_deps,  include_directories : incdir)
# executable('append', 'src/async_example.c', dependencies: zstore_deps)
