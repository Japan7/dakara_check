project('dakara_check', 'c', version: '1.0.0')

libavdeps = []
libavdeps += dependency('libavformat')
libavdeps += dependency('libavutil')

ffmpegaacsucks_dep = dependency('ffmpegaacsucks',
                                fallback: ['ffmpegaacsucks', 'ffmpegaacsucks_dep'])

version_h = vcs_tag(input: 'version.h.in', output: 'version.h')

deps = libavdeps + ffmpegaacsucks_dep

install_headers('dakara_check.h')

libdakara_check = library(
  'dakara_check',
  'libdakara_check.c',
  version_h,
  dependencies: deps,
  install: true,
)

dakara_check_exe = executable(
  'dakara_check',
  'dakara_check.c',
  dependencies: deps,
  link_with: libdakara_check,
  install: true,
)

pkg = import('pkgconfig')
pkg.generate(libdakara_check)

dakara_check_dep = declare_dependency(link_with: libdakara_check)
