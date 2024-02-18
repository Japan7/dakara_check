FROM ghcr.io/odrling/chimera:x86_64@sha256:41b1908517a31791616a7a5d6b89351d227331816ad5bbd2755ace75f40d9cee AS builder
ARG ARCH

RUN apk add base-devel-static meson clang git lld

COPY . /dakara_check

RUN cd /dakara_check && /dakara_check/ci/build.sh

FROM ghcr.io/odrling/chimera@sha256:26016bae5fc810a109cbadc449be1926594a5819223348dc9a41af20c30dc3c6

COPY --from=builder /dakara_check/dest/ /
ENTRYPOINT ["/usr/local/bin/dakara_check"]

