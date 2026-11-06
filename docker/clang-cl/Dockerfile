# sbo/clang-cl:local -- MSVC-ABI cross toolchain for SBO's Windows binaries (client.dll, hl.dll). CI builds the same
# image from this file (.github/workflows/build.yml) and rebuilds it when this file changes.
#
#   docker build -t sbo/clang-cl:local docker/clang-cl
#
#  * LLVM 19 (clang-cl, lld-link, llvm-lib, llvm-rc, llvm-mt) + CMake 3.31 + Ninja from Debian trixie, apt pinned to a
#    snapshot.debian.org date so the package versions are reproducible.
#  * MSVC CRT + Windows SDK (x86 only, desktop, incl. the /MTd debug libs) repacked by xwin into /opt/xwin (case-insensitive symlinks included).
#    xwin verifies every payload against the SHA-256 in Microsoft's VS manifest; the versions below are pinned.
#    (The VS manifest itself is still fetched live from Microsoft: if MS ever drops CRT 14.44 from manifest 17, bump
#    XWIN_CRT_VERSION.)
#  * Target triple i686-pc-windows-msvc; see cmake/toolchains/clang-cl-i686.cmake for how the paths are used.
FROM debian:trixie-20250929-slim@sha256:1caf1c703c8f7e15dcf2e7769b35000c764e6f50e4d7401c355fb0248f3ddfdb

ARG DEBIAN_SNAPSHOT=20250929T000000Z
ARG LLVM_VER=19
RUN set -eux; \
    rm -f /etc/apt/sources.list.d/debian.sources; \
    echo "deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/${DEBIAN_SNAPSHOT} trixie main" > /etc/apt/sources.list; \
    apt-get -o Acquire::Retries=3 update; \
    apt-get install -y --no-install-recommends \
      clang-${LLVM_VER} lld-${LLVM_VER} llvm-${LLVM_VER} cmake ninja-build ca-certificates curl python3 file; \
    rm -rf /var/lib/apt/lists/*
ENV PATH=/usr/lib/llvm-${LLVM_VER}/bin:$PATH

ARG XWIN_VERSION=0.10.0
ARG XWIN_SHA256=d870eb4b2f390878af6da1ccd3cf321d22fcb72720984853b4be732ae597fc88
ARG XWIN_CRT_VERSION=14.44.17.14
ARG XWIN_SDK_VERSION=10.0.26100
RUN set -eux; cd /tmp; \
    curl -fsSL --retry 3 -o xwin.tar.gz \
      https://github.com/Jake-Shadle/xwin/releases/download/${XWIN_VERSION}/xwin-${XWIN_VERSION}-x86_64-unknown-linux-musl.tar.gz; \
    echo "${XWIN_SHA256}  xwin.tar.gz" | sha256sum -c -; \
    tar xzf xwin.tar.gz; \
    ./xwin-${XWIN_VERSION}-x86_64-unknown-linux-musl/xwin --accept-license --http-retry 3 --temp \
      --manifest-version 17 --crt-version ${XWIN_CRT_VERSION} --sdk-version ${XWIN_SDK_VERSION} \
      --arch x86 --variant desktop \
      splat --include-debug-libs --output /opt/xwin; \
    rm -rf /tmp/*; \
    printf 'xwin %s\ncrt %s\nsdk %s\n' "${XWIN_VERSION}" "${XWIN_CRT_VERSION}" "${XWIN_SDK_VERSION}" > /opt/xwin/VERSIONS; \
    ls /opt/xwin /opt/xwin/crt/lib /opt/xwin/sdk/lib/um

# Debian ships no clang-cl driver binary outside clang-tools; clang picks its driver mode from argv[0]
RUN ln -s clang /usr/lib/llvm-${LLVM_VER}/bin/clang-cl && clang-cl --version

ENV XWIN_ROOT=/opt/xwin
WORKDIR /work
