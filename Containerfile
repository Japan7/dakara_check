FROM ghcr.io/odrling/chimera:cross AS builder
ARG ARCH

COPY . /dakara_check

RUN cd /dakara_check && /dakara_check/ci/build.sh

FROM ghcr.io/odrling/chimera

COPY --from=builder /dakara_check/dest/ /
ENTRYPOINT ["/usr/local/bin/dakara_check"]

