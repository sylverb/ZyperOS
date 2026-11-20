# A Linux with SDL2's offscreen video and Mesa's software OpenGL ES, for
# make check-p8fx-gl on a computer that is not Linux (tools/p8fx-gl-check.py
# --docker): the shim needs LD_PRELOAD and the GLES renderer, which macOS has
# not. Ubuntu 24.04's packages, as the Linux job of .github/workflows/check.yml.
FROM ubuntu:24.04
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
        gcc libc6-dev make pkg-config libsdl2-dev libsdl2-image-dev libegl-dev libgles-dev \
        libegl-mesa0 libgl1-mesa-dri python3 \
 && rm -rf /var/lib/apt/lists/*
