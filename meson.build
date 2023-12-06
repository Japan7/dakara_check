project('dakara_check', 'c')

libavdeps = []
libavdeps += dependency('libavformat')
libavdeps += dependency('libavutil')
libavdeps += dependency('libavcodec')

cc = meson.get_compiler('c')
ffmpegaacsucks = cc.find_library('ffmpegaacsucks', required: true)

deps = libavdeps + ffmpegaacsucks

dakara_check_exe = executable(
  'dakara_check',
  'dakara_check.c',
  dependencies: deps,
  install: true,
)
