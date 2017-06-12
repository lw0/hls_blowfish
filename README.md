# `hls_blowfish` - Example AFU Action Implementing the Blowfish Cipher

## Build

The hardware directory must be linked as `hls_blowfish` into the `$SNAP_ROOT/hardware/action_examples/` directory and should be recognized during the regular snap `make model` or `make image` process, if `$ACTION_ROOT` points to it.

The software part of the example can be built by calling `make` from the `software` directory.
