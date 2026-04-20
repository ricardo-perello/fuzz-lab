FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# ── System dependencies ───────────────────────────────────────────────────────
RUN apt-get update && apt-get install -y \
    build-essential \
    clang \
    llvm \
    llvm-dev \
    python3 \
    python3-dev \
    python3-setuptools \
    git \
    wget \
    curl \
    pkg-config \
    autoconf \
    automake \
    libtool \
    libglib2.0-dev \
    libpixman-1-dev \
    ninja-build \
    meson \
    cmake \
    zlib1g-dev \
    flex \
    bison \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# ── AFL++ ─────────────────────────────────────────────────────────────────────
RUN git clone --depth 1 https://github.com/AFLplusplus/AFLplusplus.git && \
    cd AFLplusplus && \
    make distrib && \
    make install

# ── libpng 1.2.56 source ──────────────────────────────────────────────────────
# Using GitHub mirror for reliability; SourceForge can be intermittent
RUN wget -q https://github.com/glennrp/libpng/archive/refs/tags/v1.2.56.tar.gz \
        -O libpng-1.2.56.tar.gz && \
    tar xzf libpng-1.2.56.tar.gz

# ── Copy CRC-neutralization patch ─────────────────────────────────────────────
COPY patches/ /build/patches/

# ── Instrumented build (afl-clang-fast + ASan) ────────────────────────────────
RUN cp -r libpng-1.2.56 libpng-instrumented && \
    cd libpng-instrumented && \
    patch -p0 < /build/patches/libpng-1.2.56-no-crc.patch && \
    CC=afl-clang-fast \
    CFLAGS="-fsanitize=address -g -O1" \
    LDFLAGS="-fsanitize=address" \
    ./configure --disable-shared --prefix=/build/install && \
    make -j$(nproc) && \
    make install

# ── Vanilla build (gcc, no sanitizers, no AFL++ instrumentation) ───────────────
RUN cp -r libpng-1.2.56 libpng-vanilla && \
    cd libpng-vanilla && \
    patch -p0 < /build/patches/libpng-1.2.56-no-crc.patch && \
    CC=gcc \
    CFLAGS="-g -O1" \
    ./configure --disable-shared --prefix=/build/install_vanilla && \
    make -j$(nproc) && \
    make install

# ── Copy AFL++ png dictionary into repo layout ────────────────────────────────
RUN mkdir -p /build/dictionaries && \
    cp /build/AFLplusplus/dictionaries/png.dict /build/dictionaries/png.dict

# ── Verification: dummy compile against each library ─────────────────────────
RUN printf '#include <png.h>\nint main(void){\n  png_structp p = png_create_read_struct(PNG_LIBPNG_VER_STRING,0,0,0);\n  return (p == 0);\n}\n' > /tmp/check.c && \
    afl-clang-fast -fsanitize=address -g -O1 /tmp/check.c \
        -I/build/install/include \
        /build/install/lib/libpng12.a -lz -lm \
        -o /tmp/check_instr && \
    gcc -g -O1 /tmp/check.c \
        -I/build/install_vanilla/include \
        /build/install_vanilla/lib/libpng12.a -lz -lm \
        -o /tmp/check_vanilla && \
    echo "Both library builds verified OK"

# ── Copy Makefile and dictionary; src/ and seeds/ are volume-mounted at runtime
COPY Makefile /build/Makefile

WORKDIR /build
