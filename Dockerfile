# ─── Build stage ──────────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Essential tools for v3.0.0
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    wget \
    unzip \
    libcurl4-openssl-dev \
    # Keep these ONLY if your source code still includes LLVM headers
    llvm-dev \
    libclang-dev \
    clang \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Modern CMake build pattern: -S (source) . -B (build directory) build
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j$(nproc)

# ─── Runtime stage ────────────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 \
    # Only keep these if your binary is dynamically linked to LLVM
    libclang-cpp14 \
    libllvm14 \
    patch \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy from the 'build' folder we created above
COPY --from=builder /app/build/codemedic /usr/local/bin/codemedic
COPY completions/codemedic.bash /etc/bash_completion.d/codemedic

WORKDIR /workspace

ENTRYPOINT ["codemedic"]
CMD ["--help"]
