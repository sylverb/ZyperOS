CHECKS += check-checkrun
#| make check's runner (`tools/check-run.py`) with stand-in checks: CI's parts (`--shard I/N`) hold
#| every check once and `check-i18n-shots` in each, the same from the list in any order and within
#| the longest check of each other; `check-i18n-shots`' own parts hold every language on every
#| handheld, each with English on its handhelds and the first with English on both, within one
#| Brick run of each other; never more heavy checks at once than `CHECK_HEAVY_SLOTS`, in one run or
#| with another process holding a slot, the light ones going on meanwhile, a slot taken again once
#| its holder dies, the wait said on the check's line
