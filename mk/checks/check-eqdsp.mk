CHECKS += check-eqdsp
#| the equalizer's five bands on test tones: flat touches nothing, each band lifts and cuts its own
#| tone by 6 dB, a step has no click and ends in 20 ms, the knee never cuts flat, and the cost of a
#| second of 48 kHz stereo; and the volume boost's top cells (`EQ_COMP_TAB`, cells 4 to 6): cells 1
#| to 3 and the moves between them the boost as it was to the sample (against a copy of it in the
#| check), each top cell louder than the one under it on PICO-8-like chip voices at fake08's levels
#| and a loud film, cell 6 a three-voice tune about 9 dB and all voices at full about 6 dB louder,
#| no sample past -1 dB of full scale in any cell or on the way through them, a sine's THD+N cell
#| by cell (none, then under 5%, 15% and 40%), the compressor's swing on a held tune (under 0.3 dB)
#| and on a run of notes, silence after cell 6 silence with no subnormal or NaN, its cost a frame,
#| and its table one row a top cell of src/boost.h
