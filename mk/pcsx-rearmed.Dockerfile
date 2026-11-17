# The image mk/build-pcsx-rearmed.sh builds the PlayStation core in: the
# compiler libretro's buildbot built it with, so the core ZyperOS builds
# differs from the buildbot core it shipped up to 1.0.5 by its patches and
# nothing else. That core says which compiler: its .comment is "GCC: (Ubuntu
# 12.3.0-1ubuntu1~22.04.3) 12.3.0", Ubuntu 22.04's gcc-12, run as a
# linux/arm64 container (natively on an Apple Silicon Mac).
#
#   docker build --platform linux/arm64 -f mk/pcsx-rearmed.Dockerfile -t zyperos-pcsx-rearmed mk
FROM ubuntu:22.04@sha256:9abcaffc92a5279fff3a6a41635c0800ea33e528425fb389658806476e7defde

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        gcc-12=12.3.0-1ubuntu1~22.04.3 \
        libc6-dev \
        make \
        binutils \
    && rm -rf /var/lib/apt/lists/*

# Where libretro's CI builds it (its Makefile.libretro knows the path)
WORKDIR /builds/libretro/pcsx_rearmed
