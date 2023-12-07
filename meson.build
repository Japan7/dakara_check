project('dakara_check', 'c')

libavdeps = []
libavdeps += dependency('libavformat')
libavdeps += dependency('libavutil')
libavdeps += dependency('libavcodec')

ffmpegaacsucks_dep = dependency('ffmpegaacsucks',
                                fallback: ['ffmpegaacsucks', 'ffmpegaacsucks_dep'])

deps = libavdeps + ffmpegaacsucks_dep

dakara_check_exe = executable(
  'dakara_check',
  'dakara_check.c',
  dependencies: deps,
  install: true,
)
