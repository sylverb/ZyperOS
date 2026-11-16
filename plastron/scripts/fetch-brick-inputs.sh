#!/bin/sh
# The Brick's closed files, the builder's own copy (make brick-fetch): fetched
# from where their publisher put them, checked against the packages' sums,
# into BRICK_GPU and BRICK_XR829, which make brick mounts. Never committed,
# never in an image (ZyperOS's spec, section 7: the player's own copy; the
# ZyperOS Installer fetches the same files for a player's card).
#
#   - Imagination's PowerVR DDK 1.11@5516664 userspace for the GE8300 and the
#     GPU's firmware, and XRadio's XR829 firmware: Creality's Sonic Pad SDK at
#     7f37a7f (package/libs/libgpu/ge8300/fbdev/glibc/lib64,
#     package/firmware/linux-firmware/xr829), checked against
#     package/pvr-ge8300/files.sha256 and package/xr829-firmware/files.sha256.
#   - The Khronos headers the libraries are built against (EGL, KHR, GLES,
#     GLES2, GLES3), from Khronos's own registries at a pinned commit; they
#     are Apache-2.0 and MIT, not closed, but the DDK ships none.
#
# Runs on the computer that builds (curl, and sha256sum or shasum).
set -eu
HERE=$(cd "$(dirname "$0")/.." && pwd)
GPU=${BRICK_GPU:-$HOME/brick-runtime/gpu-ddk-1.11}
XR=${BRICK_XR829:-$HOME/brick-runtime/xr829}
SDK=https://raw.githubusercontent.com/CrealityTech/sonic_pad_os/7f37a7fbf4ad59907034715e3d967b9177de1cd4
EGL=https://raw.githubusercontent.com/KhronosGroup/EGL-Registry/8e56e86e65687697ef54114cdb1afabdfac756bf/api
GL=https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/6af574a14089ccfee87efe230ebcdd8742859813/api

sum() { if command -v sha256sum > /dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
get() {
	mkdir -p "$(dirname "$2")"
	curl -fsSL --retry 3 -o "$2.part" "$1" && mv "$2.part" "$2" ||
		{ rm -f "$2.part"; echo "fetch-brick-inputs: could not fetch $1" >&2; exit 1; }
}

# The closed ones, by their sums' list: each file at its path under the SDK's
# folder, checked before anything else uses it
while read -r s f; do
	get "$SDK/package/libs/libgpu/ge8300/fbdev/glibc/$f" "$GPU/$f"
done < "$HERE/buildroot/package/pvr-ge8300/files.sha256"
while read -r s f; do
	get "$SDK/package/firmware/linux-firmware/xr829/$f" "$XR/$f"
done < "$HERE/buildroot/package/xr829-firmware/files.sha256"
(cd "$GPU" && sum -c "$HERE/buildroot/package/pvr-ge8300/files.sha256") > /dev/null ||
	{ echo "fetch-brick-inputs: a GPU file is not the one plastron checks" >&2; exit 1; }
(cd "$XR" && sum -c "$HERE/buildroot/package/xr829-firmware/files.sha256") > /dev/null ||
	{ echo "fetch-brick-inputs: an XR829 file is not the one plastron checks" >&2; exit 1; }

for h in EGL/egl.h EGL/eglext.h EGL/eglplatform.h KHR/khrplatform.h; do get "$EGL/$h" "$GPU/include/$h"; done
for h in GLES/gl.h GLES/glext.h GLES/glplatform.h GLES2/gl2.h GLES2/gl2ext.h GLES2/gl2platform.h \
	GLES3/gl3.h GLES3/gl31.h GLES3/gl32.h GLES3/gl3platform.h; do
	get "$GL/$h" "$GPU/include/$h"
done
echo "fetch-brick-inputs: the GPU's files in $GPU, the XR829's in $XR, all as plastron checks them"
