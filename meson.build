project(
  'earblaster',
  'cpp',
  version: '0.1.2',
  license: 'Unlicense',
  meson_version: '>=0.56',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
    'prefix=/usr',
  ],
)

gtkmm = dependency('gtkmm-3.0')
gst = dependency('gstreamer-1.0')
gst_tag = dependency('gstreamer-tag-1.0')
gst_pbutils = dependency('gstreamer-pbutils-1.0')
taglib = dependency('taglib')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('APP_ID', 'org.gmgauthier.EarBlaster')
conf.set_quoted('DATADIR', get_option('prefix') / get_option('datadir') / 'earblaster')
conf.set_quoted('SOURCE_ROOT', meson.project_source_root())

configure_file(
  output: 'config.hpp',
  configuration: conf,
)

inc = include_directories('.', 'src')

executable(
  'earblaster',
  'src/main.cpp',
  'src/application.cpp',
  'src/paths.cpp',
  'src/main_window.cpp',
  'src/seal_view.cpp',
  'src/about_dialog.cpp',
  'src/player.cpp',
  'src/playlist.cpp',
  'src/cover_art.cpp',
  'src/settings.cpp',
  'src/eq_window.cpp',
  'src/prefs_window.cpp',
  include_directories: inc,
  dependencies: [gtkmm, gst, gst_tag, gst_pbutils, taglib],
  install: true,
  install_dir: get_option('bindir'),
)

install_data(
  'data/earblaster.desktop',
  install_dir: get_option('datadir') / 'applications',
)

install_data(
  'data/skin/lcos/lcos.css',
  install_dir: get_option('datadir') / 'earblaster' / 'skin' / 'lcos',
)

install_data(
  'brand/mark-ring-bolt.svg',
  'brand/icon-tile.svg',
  'brand/lockup-pill.svg',
  install_dir: get_option('datadir') / 'earblaster' / 'brand',
)

install_data(
  'brand/icon-tile.svg',
  rename: 'earblaster.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
