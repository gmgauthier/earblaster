project(
  'earblaster',
  'cpp',
  version: '1.1.8',
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
  'src/cue_sheet.cpp',
  'src/cover_art.cpp',
  'src/settings.cpp',
  'src/eq_window.cpp',
  'src/prefs_window.cpp',
  'src/sync_devices.cpp',
  'src/sync_window.cpp',
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
  'data/skin/lcos/btn-prev.svg',
  'data/skin/lcos/btn-play.svg',
  'data/skin/lcos/btn-pause.svg',
  'data/skin/lcos/btn-stop.svg',
  'data/skin/lcos/btn-next.svg',
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

earblaster_tests = executable(
  'earblaster-tests',
  'tests/test_cue.cpp',
  'src/cue_sheet.cpp',
  include_directories: inc,
  dependencies: [gtkmm],
)
test('cue', earblaster_tests)

player_tests = executable(
  'earblaster-player-tests',
  'tests/test_player.cpp',
  'src/player.cpp',
  include_directories: inc,
  dependencies: [gtkmm, gst, gst_tag],
)
test('player', player_tests)

playlist_tests = executable(
  'earblaster-playlist-tests',
  'tests/test_playlist.cpp',
  'src/playlist.cpp',
  'src/cue_sheet.cpp',
  'src/cover_art.cpp',
  include_directories: inc,
  dependencies: [gtkmm, gst, gst_tag, gst_pbutils, taglib],
)
test('playlist', playlist_tests)

settings_tests = executable(
  'earblaster-settings-tests',
  'tests/test_settings.cpp',
  'src/settings.cpp',
  include_directories: inc,
  dependencies: [gtkmm],
)
test('settings', settings_tests)
