tests = [
  'config',
  'utf8'
]

foreach test_file : tests
  t = executable(
    test_file,
    files(test_file + '.c', 'tap.c'), common_sources, wl_proto_src, wl_proto_headers,
    include_directories: ['../src'],
    dependencies: [librt, libm, freetype, harfbuzz, cairo, pangocairo, wayland_client, xkbcommon, glib, gio_unix],
    install: false
    )

  test(test_file, t, protocol: 'tap')
endforeach
