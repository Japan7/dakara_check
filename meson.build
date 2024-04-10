project(
    'dakara_check',
    'c',
    version: '4.0.1',
    default_options: ['warning_level=3', 'c_std=gnu17'],
)

git = find_program('git', required: false)

if git.found()
    # avoids detection issues in some cases, such as git worktrees
    version_h = vcs_tag(
        command: [git, 'describe', '--dirty=+', '--always'],
        input: 'version.h.in',
        output: 'version.h',
    )
else
    version_h = vcs_tag(input: 'version.h.in', output: 'version.h')
endif

deps = []
deps += dependency('libavformat', fallback: ['ffmpeg', 'libavformat_dep'])
deps += dependency('libavutil', fallback: ['ffmpeg', 'libavutil_dep'])

deps += dependency(
    'ffmpegaacsucks',
    version: '>=2.0.0',
    fallback: ['ffmpegaacsucks', 'ffmpegaacsucks_dep'],
)

install_headers('dakara_check.h')

libdakara_check = library(
    'dakara_check',
    'libdakara_check.c',
    version_h,
    dependencies: deps,
    install: true,
    version: meson.project_version(),
    soversion: '3',
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
