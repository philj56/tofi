project(
  'tofi',
  'c',
  version: '0.7.0',
  license: 'MIT',
  meson_version: '>=0.61.0',
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

config_location = join_paths(
  get_option('sysconfdir'),
  'xdg',
  'tofi'
)

install_data(
  'doc/config',
  install_dir: config_location
)

license_dir = join_paths(
  get_option('datadir'),
  'licenses',
  'tofi'
)

install_data(
  'LICENSE',
  install_dir: license_dir
)

completion_location = join_paths(
  get_option('prefix'),
  get_option('datadir'),
  'bash-completion',
  'completions'
)

install_data(
  'completions/tofi',
  install_dir: completion_location
)

install_symlink(
  'tofi-run',
  install_dir: completion_location,
  pointing_to: 'tofi',
)

install_symlink(
  'tofi-drun',
  install_dir: completion_location,
  pointing_to: 'tofi',
)

install_symlink(
  'tofi-run',
  install_dir: get_option('bindir'),
  pointing_to: 'tofi',
)

install_symlink(
  'tofi-drun',
  install_dir: get_option('bindir'),
  pointing_to: 'tofi',
)

add_project_arguments(
  [
    '-pedantic',
    #'-Wconversion',
    '-Wshadow',
    '-Wno-unused-parameter',
    '-D_GNU_SOURCE',
    '-D_FORTIFY_SOURCE=2',
  ],
  language: 'c'
)

common_sources = files(
  'src/clipboard.c',
  'src/color.c',
  'src/compgen.c',
  'src/config.c',
  'src/desktop_vec.c',
  'src/drun.c',
  'src/entry.c',
  'src/entry_backend/pango.c',
  'src/entry_backend/harfbuzz.c',
  'src/fuzzy_match.c',
  'src/history.c',
  'src/input.c',
  'src/lock.c',
  'src/log.c',
  'src/mkdirp.c',
  'src/shm.c',
  'src/string_vec.c',
  'src/surface.c',
  'src/unicode.c',
  'src/wlr-layer-shell-unstable-v1.c',
  'src/xmalloc.c',
)

compgen_sources = files(
  'src/main_compgen.c',
  'src/compgen.c',
  'src/fuzzy_match.c',
  'src/log.c',
  'src/mkdirp.c',
  'src/string_vec.c',
  'src/unicode.c',
  'src/xmalloc.c'
)

cc = meson.get_compiler('c')
librt = cc.find_library('rt', required: false)
libm = cc.find_library('m', required: false)
freetype = dependency('freetype2')
harfbuzz = dependency('harfbuzz')
cairo = dependency('cairo')
pangocairo = dependency('pangocairo')
wayland_client = dependency('wayland-client')
wayland_protocols = dependency('wayland-protocols', native: true)
wayland_scanner_dep = dependency('wayland-scanner', native: true)
xkbcommon = dependency('xkbcommon')
glib = dependency('glib-2.0')
gio_unix = dependency('gio-unix-2.0')

if wayland_client.version().version_compare('<1.20.0')
  add_project_arguments(
    ['-DNO_WL_OUTPUT_NAME=1'],
    language: 'c'
    )
endif

if harfbuzz.version().version_compare('<4.4.0')
  add_project_arguments(
    ['-DNO_HARFBUZZ_FONT_CHANGED=1'],
    language: 'c'
    )
endif



# Generate the necessary Wayland headers / sources with wayland-scanner
wayland_scanner = find_program(
  wayland_scanner_dep.get_variable(pkgconfig: 'wayland_scanner'),
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

subdir('test')

executable(
  'tofi',
  files('src/main.c'), common_sources, wl_proto_src, wl_proto_headers,
  dependencies: [librt, libm, freetype, harfbuzz, cairo, pangocairo, wayland_client, xkbcommon, glib, gio_unix],
  install: true
)

executable(
  'tofi-compgen',
  compgen_sources,
  dependencies: [glib],
  install: false
)

scdoc = find_program('scdoc', required: get_option('man-pages'))
if scdoc.found()
  sed = find_program('sed')
  sh = find_program('sh')
  mandir = get_option('mandir')

  output = 'tofi.1'
  custom_target(
    output,
    input: 'doc/tofi.1.scd',
    output: output,
    command: [
      sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc.full_path(), output)
    ],
    install: true,
    install_dir: '@0@/man1'.format(mandir)
  )

  install_symlink(
    'tofi-run.1',
    install_dir: '@0@/man1'.format(mandir),
    pointing_to: 'tofi.1'
  )

  install_symlink(
    'tofi-drun.1',
    install_dir: '@0@/man1'.format(mandir),
    pointing_to: 'tofi.1'
  )


  output = 'tofi.5'
  custom_target(
    output,
    input: 'doc/tofi.5.scd',
    output: output,
    command: [
      sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc.full_path(), output)
    ],
    install: true,
    install_dir: '@0@/man5'.format(mandir)
  )
endif
