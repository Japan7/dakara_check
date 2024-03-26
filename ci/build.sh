#!/bin/sh
export LDFLAGS="-Wl,-Bsymbolic"

if [ "$ARCH" != x86_64 ]; then
    extra_args="--cross-file ci/$ARCH-chimera-linux-musl.txt"
fi

meson setup build --buildtype release --strip -Ddefault_library=static -Dprefer_static=true -Dffmpeg:programs=disabled -Dffmpeg:tests=disabled -Dffmpeg:encoders=disabled -Dffmpeg:muxers=disabled -Dffmpeg:avfilter=disabled -Dffmpeg:avdevice=disabled -Dffmpeg:postproc=disabled -Dffmpeg:swresample=disabled -Dffmpeg:swscale=disabled -Dffmpeg:decoders=disabled -Dffmpeg:aac_decoder=enabled -Dffmpeg:aac_fixed_decoder=enabled -Dffmpeg:aac_latm_decoder=enabled -Dffmpeg:version3=enabled $extra_args

meson compile -C build
meson install -C build --destdir=../dest/
