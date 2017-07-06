#!/bin/bash

#
# Copyright 2016, 2017 International Business Machines
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Simple tests for example snap actions.
#

verbose=0
snap_card=0

export PATH=$PATH:./tools:./examples

function usage() {
    echo "Usage:"
    echo "  test_<action_type>.sh"
    echo "    [-C <card>]        card to be used for the test"
    echo "    [-t <trace_level>] enable tracing"
    echo "    [-h]               display help"
    echo
}
while getopts ":C:t:h" opt; do
    case $opt in
        C)
        snap_card=$OPTARG;
        ;;
        t)
        export SNAP_TRACE=$OPTARG;
        ;;
        h)
        usage;
        exit 0;
        ;;
        \?)
        echo "Invalid option: -$OPTARG" >&2
        ;;
    esac
done

blowfish_test () {
    local blocks=$1

    echo "Testing data with $blocks block(s) of each 64 bytes"
    dd if=/dev/urandom of=input.bin count=${blocks} bs=64 2> /dev/null
    dd if=/dev/urandom of=key.bin count=1 bs=16 2> /dev/null
    sync

    # Encrypt
    snap_blowfish -k key.bin -i input.bin -o encrypted.bin
    if [ $? -ne 0 ]; then
	echo "ERR: encrypt";
	exit -1;
    fi
    
    # Decrypt
    snap_blowfish -k key.bin -d -i encrypted.bin -o decrypted.bin
    if [ $? -ne 0 ]; then
        echo "ERR: encrypt";
	exit -2;
    fi
    
    diff input.bin decrypted.bin
    if [ $? -ne 0 ]; then
	echo "ERR: Data does not match!"
	hexdump input.bin > input.hex
	hexdump decrypted.bin > decrypted.hex
	echo "Use for analyis:"
	echo "  meld input.hex decrypted.hex &"
	exit -3
    else
	echo "SUCCESS: Data matches decyphered data matches the original!!!!"
    fi
}

for blocks in 1 16 32 128 ; do
    blowfish_test $blocks
done

exit 0
