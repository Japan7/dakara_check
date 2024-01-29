FROM alpine AS builder 
RUN apk add musl-dev meson clang compiler-rt git linux-headers lld tar

COPY . /dakara_check

RUN cd /dakara_check && /dakara_check/ci/build.sh

FROM alpine

COPY --from=builder /dakara_check/dest/ /
ENTRYPOINT ["/usr/local/bin/dakara_check"]

