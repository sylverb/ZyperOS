/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_ALSATRY_H
#define ZYPEROS_ALSATRY_H

/* `zyperos.elf --alsa-try [PCM]`: whether your PICO-8 would get its sound
 * through the ALSA chain this process is given (ALSA_CONFIG_PATH, as
 * launch.sh gives the port's), opened as SDL 2's ALSA backend opens it for
 * what pico8_64 asks (22050 Hz, 16-bit, one channel, 1024 frames a period,
 * or 2048 or 512): the same calls in the same order. launch.sh runs it
 * before your PICO-8 plays into a USB-C DAC's chain, and leaves PICO-8 in
 * the mix, as before, when it fails (DESIGN.md 11.55). rc.4 showed why: a
 * chain with a softvol stage in it (the DAC's has one) opened at 44100 Hz
 * stereo and failed at PICO-8's own period ("Couldn't set hardware audio
 * parameters"), and PICO-8 had no sound at all.
 *
 * Exit 0 when every ask opened, 1 at the first that did not (said on
 * stderr, which launch.sh sends to the log, with the call that failed), 2
 * with no ALSA in this build (a Mac). */
int alsa_try_main(int argc, char **argv);

#define ALSA_TRY_ARG "--alsa-try"

#endif
