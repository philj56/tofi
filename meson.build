project(
  'tofi',
  'c',
  license: 'MIT',
  default_options: [
    'c_std=c2x',
    'optimization=3',
    'buildtype=debugoptimized',
    'warning_level=3',
    'b_lto=true',
    'b_lto_threads=-1',
    'b_pie=true',
    'prefix=/usr'
  ],
)

debug = get_option('buildtype').startswith('debug')
if debug
  add_project_arguments('-DDEBUG', language : 'c')
endif

data_location = join_paths(
  get_option('prefix'),
  get_option('datadir'),
  'tofi'
)

shader_location = join_paths(
  debug ? '' : data_location,
  'shaders',
  ''
)

completion_location = join_paths(
  get_option('prefix'),
  get_option('datadir'),
  'bash-completion',
  'completions'
)

install_subdir(
  'shaders',
  install_dir: data_location
)

install_data(
  'completions/tofi',
  install_dir: completion_location
)

add_project_arguments(
  [
    '-pedantic',
    #'-Wconversion',
    '-Wshadow',
    '-Wno-unused-parameter',
    '-D_POSIX_C_SOURCE=200809L',
    '-DSHADER_PATH="@0@"'.format(shader_location),
  ],
  language: 'c'
)

sources = files(
  'src/main.c',
  'src/color.c',
  'src/compgen.c',
  'src/egl.c',
  'src/entry.c',
  'src/gl.c',
  'src/image.c',
  'src/ipc.c',
  'src/log.c',
  'src/string_vec.c',
  'src/surface.c',
  'src/wlr-layer-shell-unstable-v1.c',
)

cc = meson.get_compiler('c')
epoxy = dependency('epoxy')
glib = dependency('glib-2.0')
json = dependency('json-c')
pangocairo = dependency('pangocairo')
png = dependency('libpng')
wayland_client = dependency('wayland-client')
wayland_egl = dependency('wayland-egl')
wayland_protocols = dependency('wayland-protocols', native: true)
wayland_scanner_dep = dependency('wayland-scanner', native: true)
xkbcommon = dependency('xkbcommon')


# Generate the necessary Wayland headers / sources with wayland-scanner
wayland_scanner = find_program(
  wayland_scanner_dep.get_pkgconfig_variable('wayland_scanner'),
  native: true
)

wayland_protocols_dir = wayland_protocols.get_variable(pkgconfig: 'pkgdatadir')

wl_proto_headers = []
wl_proto_src = []
wl_proto_xml = [
  wayland_protocols_dir + '/stable/xdg-shell/xdg-shell.xml',
]

foreach proto : wl_proto_xml
  wl_proto_headers += custom_target(
    proto.underscorify() + '_client_header',
    output: '@BASENAME@.h',
    input: proto,
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'])

  wl_proto_src += custom_target(
    proto.underscorify() + '_private_code',
    output: '@BASENAME@.c',
    input: proto,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'])
endforeach

executable(
  'tofi',
  sources, wl_proto_src, wl_proto_headers,
  dependencies: [epoxy, json, glib, pangocairo, png, wayland_egl, xkbcommon],
  install: true
)

scdoc = find_program('scdoc', required: get_option('man-pages'))
if scdoc.found()
  sed = find_program('sed')
  sh = find_program('sh')
  mandir = get_option('mandir')
  manpage = 'doc/tofi.1.scd'
  output = 'tofi.1'
  custom_target(
    output,
    input: manpage,
    output: output,
    command: [
      sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc.path(), output)
    ],
    install: true,
    install_dir: '@0@/man1'.format(mandir)
  )
endif
