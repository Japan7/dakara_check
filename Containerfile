ARG ARCH
FROM ghcr.io/odrling/gentoo-crossdev-images:$ARCH-llvm17 AS builder 

COPY . /dakara_check

ARG ARCH
RUN cd /dakara_check && /dakara_check/ci/build.sh --cross-file ci/$ARCH-unknown-linux-musl.txt

FROM alpine

COPY --from=builder /dakara_check/dest/ /
ENTRYPOINT ["/usr/local/bin/dakara_check"]

