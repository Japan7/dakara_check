project(
    'dakara_check',
    'c',
    license: 'LGPL-3.0-only',
    version: '5.0.0',
    default_options: ['warning_level=3', 'c_std=gnu17'],
)

git = find_program('git', required: false)

# avoids detection issues in some cases, such as git worktrees
if git.found()
    version_h = vcs_tag(
        command: [git, 'describe', '--dirty=+', '--always'],
        input: 'version.h.in',
        output: 'version.h',
    )
else
    version_h = vcs_tag(input: 'version.h.in', output: 'version.h')
endif

deps = []
deps += dependency('libavformat')
deps += dependency('libavcodec')
deps += dependency('libavutil')
deps += dependency(
    'libass',
    fallback: 'libass',
    default_options: ['require-system-font-provider=false'],
)

install_headers('include/dakara_check.h')
inc = include_directories('include')

libdakara_check = library(
    'dakara_check',
    'libdakara_check.c',
    version_h,
    dependencies: deps,
    include_directories: inc,
    install: true,
    version: meson.project_version(),
    soversion: '4',
)

if get_option('programs')
    executable(
        'dakara_check',
        'dakara_check.c',
        dependencies: deps,
        include_directories: inc,
        link_with: libdakara_check,
        install: true,
    )

    executable(
        'dakara_check_sub',
        'dakara_check_sub.c',
        dependencies: deps,
        include_directories: inc,
        link_with: libdakara_check,
        install: true,
    )
endif

pkg = import('pkgconfig')
pkg.generate(libdakara_check)

dakara_check_dep = declare_dependency(
    link_with: libdakara_check,
    dependencies: deps,
    include_directories: inc,
)
