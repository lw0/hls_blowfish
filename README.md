# `hls_blowfish` - Example AFU Action Implementing the Blowfish Cipher

## Build

The hardware directory must be copied into `$SNAP_ROOT/hardware/action_examples/hls_blowfish/` and should be recognized during the regular snap `make model` or `make image` process, if `$ACTION_ROOT` points to it.

The software part of the example can be built by calling `make` from the `software` directory, given that the following symlinks are set up there:
  * `config.mk` -> `$SNAP_ROOT/software/config.mk`
  * `include/` -> `$SNAP_ROOT/software/include/`
  * `lib/` -> `$SNAP_ROOT/software/lib/`

HLS CFLAGS we have set to:
  `-DNO_SYNTH -I../../../../../bb/u/haver/framework/snap/hardware/action_examples/include -I../../../../../bb/u/haver/framework/snap/hardware/action_examples/hls_blowfish -I../../../../../bb/u/haver/framework/snap/software/examples -I../../../../../bb/u/haver/framework/snap/software/include -Wall -W -O2 -Wno-unknown-pragmas`

This is certainly not optimal, but worked for me for my initial experiment. It of course needs to be adjusted to your needs.
