FROM ubuntu:26.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        wget \
        git \
        unzip \
        zip \
        xz-utils \
        build-essential \
        ccache \
        cmake \
        ninja-build \
        python3 \
        python3-yaml \
        openjdk-21-jdk \
        clang \
        clang-format \
        libclang-dev \
        llvm \
        lld \
        gdb \
        time && \
    clang_major="$(clang -dumpversion | cut -d. -f1)" && \
    apt-get install -y --no-install-recommends \
        "libclang-rt-${clang_major}-dev" \
        "libclang-common-${clang_major}-dev" && \
    rm -rf /var/lib/apt/lists/*

# Expose the LLVM toolchain on PATH (llvm-symbolizer is picked up by sanitizers)
RUN llvm_bin="$(ls -d /usr/lib/llvm-*/bin | sort -V | tail -n1)" && \
    for tool in "${llvm_bin}"/*; do \
        name="$(basename "${tool}")" && \
        [ -e "/usr/local/bin/${name}" ] || ln -s "${tool}" "/usr/local/bin/${name}"; \
    done

RUN mkdir -p /third-party && cd /third-party && \
    wget --no-check-certificate -q https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz && \
    tar --no-same-owner --no-same-permissions -xzf v1.14.0.tar.gz && \
    rm v1.14.0.tar.gz

RUN mkdir -p /usr/local/ccache-bin &&                     \
    CCACHE_PATH=$(which ccache) &&                        \
    for binary in gcc g++ clang clang++ cc c++; do        \
        ln -s "$CCACHE_PATH" "/usr/local/ccache-bin/$binary"; \
    done

ENV PATH="/usr/local/ccache-bin:${PATH}"

ENV CC=clang \
    CXX=clang++ \
    CCACHE_DIR=/build_cache/ccache \
    CCACHE_MAXSIZE=2G \
    CCACHE_COMPRESS=1 \
    CCACHE_COMPILERCHECK=content \
    CCACHE_SLOPPINESS=time_macros,include_file_mtime,file_stat_matches \
    CCACHE_BASEDIR=/workspace \
    LOCAL_GOOGLETESTS_PATH=/third-party/googletest-1.14.0/

WORKDIR /workspace

CMD ["/bin/bash"]
