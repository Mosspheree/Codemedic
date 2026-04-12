# ─── Build stage ──────────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# We need the -dev packages here so CMake can find the headers and config files
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    wget \
    xz-utils \
    libcurl4-openssl-dev \
    llvm-dev \
    libclang-dev \
    clang \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

# ─── Runtime stage ────────────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Runtime only needs the libraries, not the -dev headers or build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 \
    libclang-cpp14 \
    libllvm14 \
    patch \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/build/codemedic /usr/local/bin/codemedic
COPY completions/codemedic.bash /etc/bash_completion.d/codemedic

WORKDIR /workspace

ENTRYPOINT ["codemedic"]
CMD ["--help"]
