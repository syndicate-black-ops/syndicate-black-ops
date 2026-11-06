# sbo/analysis:local -- static analysis tools for scripts/check_sanitizers.sh (clang-tidy, clang static analyzer via
# clang-tidy's clang-analyzer-* checks, cppcheck), with 32-bit glibc / libstdc++ headers so the game code is parsed
# with the same -m32 flags as the sniper build.
#
#   docker build -t sbo/analysis:local docker/analysis
#
# Same Debian snapshot as docker/clang-cl, so the package versions are reproducible.
FROM debian:trixie-20250929-slim@sha256:1caf1c703c8f7e15dcf2e7769b35000c764e6f50e4d7401c355fb0248f3ddfdb

ARG DEBIAN_SNAPSHOT=20250929T000000Z
ARG LLVM_VER=19
RUN set -eux; \
    rm -f /etc/apt/sources.list.d/debian.sources; \
    echo "deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/${DEBIAN_SNAPSHOT} trixie main" > /etc/apt/sources.list; \
    apt-get -o Acquire::Retries=3 update; \
    apt-get install -y --no-install-recommends \
      clang-${LLVM_VER} clang-tidy-${LLVM_VER} clang-tools-${LLVM_VER} cppcheck cmake ninja-build python3 \
      gcc-multilib g++-multilib libc6-dev-i386; \
    rm -rf /var/lib/apt/lists/*
ENV PATH=/usr/lib/llvm-${LLVM_VER}/bin:$PATH
WORKDIR /work
