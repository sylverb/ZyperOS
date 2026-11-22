CHECKS += check-eqplugin
#| the ports' equalizer plugin (plastron's zyperos-eq) built for this computer and loaded as
#| alsa-lib's ladspa PCM loads it: no file and flat untouched, Voice +3 dB at 1 kHz, the launcher's
#| file followed and a half-written one not taken, its cut (your PICO-8 quiet while MP4 plays)
#| silence after a period and 20 ms, faded, never louder, a stream starting cut silent and cleared
#| the sound back to the bit, the volume boost's gain written while it runs (portkeys's) 6 dB
#| louder, cell 3's gain alone 9 dB, the most (the top cells, src/eqdsp.h's EQ_COMP_TAB) louder
#| still with every peak under -1 dB of full scale, back to 0 the sound to the bit, and its cost;
#| then on Linux a port's whole chain (`sd/zyperos/eq-port.sh`) opened as SDL 2 opens it, at 22050
#| Hz mono and stereo, 44100, 48000, 32000 and 11025, over a stand-in for the Pixel 2's dmix with
#| its limits (`tools/eqchain-check.sh`, `tools/pcm-dmixlike.c`), the system's default first to
#| show the stand-in holds, the boost's gain written half way through a tone played as PICO-8 plays
#| (22050 Hz mono, `tools/eqchain-gain.c`) heard by that many dB in what reaches the mix, and a
#| loud cart through it at cell 3 then at the most, louder at the most by 3 dB or more and never
#| past -1 dB of full scale (tools/eqplugin-alsa.sh runs it through the handheld's own alsa-lib, by
#| hand)
