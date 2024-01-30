FROM docker.io/amd64/alpine AS bootstrap
RUN apk add tar gzip curl
RUN curl https://repo.chimera-linux.org/live/20240122/chimera-linux-x86_64-ROOTFS-20240122-bootstrap.tar.gz -o chimera.tar.gz && \
    echo 'b57b4c84ef1b8c5c628e84aa9f1b80863d748440643041414102b9ee19b0a5e4  chimera.tar.gz' | sha256sum -c && \
    mkdir /chimera && tar xf chimera.tar.gz -C /chimera

FROM scratch AS builder 
ARG ARCH

COPY --from=bootstrap /chimera /

RUN apk add musl-devel meson clang clang-rt-devel llvm-devel-static git lld

COPY . /dakara_check

RUN cd /dakara_check && /dakara_check/ci/build.sh

FROM alpine

COPY --from=builder /dakara_check/dest/ /
ENTRYPOINT ["/usr/local/bin/dakara_check"]

