# Big Bucko Breakout

This is the source code and assets for my entry for Buckojam 2026. [Itch page](https://bobsayshilol.itch.io/big-bucko-breakout)

The code in this repo is based on https://github.com/kasamikona/loopy-homebrew-template and wouldn't have been possible without it.

## Building

Every build needs the data files to be baked. To do this run `make data`.

For a Loopy ROM build, follow the steps at https://github.com/kasamikona/loopy-homebrew-template to get a working compiler (or just build `GCC` from source targeting `SuperH`) and use that when running `make`.

For a native/web build use the `build_web.sh` script.

## Running

[LoopyMSE](https://github.com/LoopyMSE/LoopyMSE) is your best bet for running this, though the build also produces a byteswapped ROM for running in `MAME`.

Note that the profiling code won't work outside of a custom LoopyMSE build since it requires a fake `rdtsc`-like instruction.
