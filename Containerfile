ARG BUILDER_IMAGE=ghcr.io/odrling/chimera:cross
FROM ${BUILDER_IMAGE} AS builder
ARG ARCH

COPY . /dakara_check

RUN cd /dakara_check && /dakara_check/ci/build.sh

FROM ghcr.io/odrling/chimera

COPY --from=builder /dakara_check/dest/ /
ENTRYPOINT ["/usr/local/bin/dakara_check"]

