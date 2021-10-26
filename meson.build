project(
  'greetd-mini-wl-greeter',
  'c',
  license: 'MIT',
  default_options: [
    'c_std=c2x',
    'optimization=3',
    'buildtype=release',
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
  'greetd-mini-wl-greeter'
)

add_project_arguments(
  [
    '-pedantic',
    #'-Wconversion',
    '-Wshadow',
    '-Wno-unused-parameter',
    '-D_POSIX_C_SOURCE=200809L',
  ],
  language: 'c'
)

sources = files(
  'src/main.c',
  'src/log.c',
  'src/egl.c',
  'src/xdg-shell-protocol.c',
)

cc = meson.get_compiler('c')
epoxy = dependency('epoxy')
rt = cc.find_library('rt')
wayland_client = dependency('wayland-client')
wayland_egl = dependency('wayland-egl')
xkbcommon = dependency('xkbcommon')

executable(
  'greetd-mini-wl-greeter',
  sources,
  dependencies: [epoxy, rt, wayland_client, wayland_egl, xkbcommon],
  install: true
)

# scdoc = find_program('scdoc', required: get_option('man-pages'))
# if scdoc.found()
#   sed = find_program('sed')
#   sh = find_program('sh')
#   mandir = get_option('mandir')
#   manpage = 'doc/greetd-mini-greeter.1.scd'
#   output = 'greetd-mini-greeter.1'
#   custom_target(
#     output,
#     input: manpage,
#     output: output,
#     command: [
#       sh, '-c', '@0@ < @INPUT@ | sed "s|CSS_PATH|@1@|g;" > @2@'.format(scdoc.path(), css_location, output)
#     ],
#     install: true,
#     install_dir: '@0@/man1'.format(mandir)
#   )
# endif
