# ─── Build stage ──────────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# ADDED: git, wget, and xz-utils so FetchContent works
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libcurl4-openssl-dev \
    ca-certificates \
    git \
    wget \
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

# ─── Runtime stage ────────────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    clang \
    gcc \
    g++ \
    libcurl4 \
    patch \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/build/codemedic /usr/local/bin/codemedic

# Shell completions
COPY completions/codemedic.bash /etc/bash_completion.d/codemedic

WORKDIR /workspace

ENTRYPOINT ["codemedic"]
CMD ["--help"]
