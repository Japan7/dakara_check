#!/bin/sh
export PATH="$PATH:/usr/lib/llvm/17/bin"
export CC=clang
export CC_LD=lld
export UBSAN_OPTIONS="signed-integer-overflow"
export CFLAGS="-D_FORTIFY_SOURCE=3"
export LDFLAGS="-Wl,-O1 -Wl,--as-needed"

meson setup build --buildtype release --strip -Ddefault_library=static -Dprefer_static=true -Dffmpeg:programs=disabled -Dffmpeg:tests=disabled -Dffmpeg:encoders=disabled -Dffmpeg:muxers=disabled -Dffmpeg:avfilter=disabled -Dffmpeg:avdevice=disabled -Dffmpeg:postproc=disabled -Dffmpeg:swresample=disabled -Dffmpeg:swscale=disabled -Dffmpeg:decoders=disabled -Dffmpeg:aac_decoder=enabled -Dffmpeg:aac_fixed_decoder=enabled -Dffmpeg:aac_latm_decoder=enabled -Dffmpeg:version3=enabled "$@"

meson compile -C build
meson install -C build --destdir=../dest/
