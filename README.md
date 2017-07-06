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

Please use:

    make install

to copy the code into the SNAP framework directory tree. Have a look into the Makefile to ensure that the target path is correct.

Here is how you can use the example application. It can encrypt and decrypt data. Please ensure that the size of the data is properly aligned. Currently using multiples of 64 bytes work well. Anything else is probably a little experimental.
````
bash-4.1$ ./snap/software/examples/snap_blowfish -h
Usage: ./snap/software/examples/snap_blowfish [-h] [-v, --verbose] [-V, --version]
  -C, --card <cardno> can be (0...3)
  -i, --input <file.bin>    input
  -o, --output <file.bin>   output
  -k, --key <file.bin>      key
  -d, --decrypt
  -t, --timeout             timeout (sec)
Example:
  snap_blowfish ...

bash-4.1$ 
````
