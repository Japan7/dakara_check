FROM ghcr.io/odrling/chimera:x86_64 AS builder
ARG ARCH

RUN apk add base-devel-static meson clang git lld

COPY . /dakara_check

RUN cd /dakara_check && /dakara_check/ci/build.sh

FROM ghcr.io/odrling/chimera

COPY --from=builder /dakara_check/dest/ /
ENTRYPOINT ["/usr/local/bin/dakara_check"]

