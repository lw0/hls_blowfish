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
# Example how to simulate the code for an AlphaDAata KU3 card.
#

echo "****************************************************************************************"
ulimit -c 10
export PATH=$PATH:/usr/local/bin
echo "****************************************************************************************"

#
# Checking out the pslse and the snap-example-actions git is not working out, due to the
# desire to use different branches from each of those. So use the brute force way for now.
#
echo "Get and build Simulation Software ..."
if [ ! -d pslse ]; then
	rm -rf pslse
	git clone https://github.com/ibm-capi/pslse pslse
	make -C pslse/libcxl
	make -C snap clean
fi

echo "****************************************************************************************"
echo "Vivado SETUP ..."
echo "****************************************************************************************"
export          XILINX_ROOT=/afs/bb/proj/fpga/xilinx
export XILINXD_LICENSE_FILE=2100@pokwinlic1.pok.ibm.com
export          VIV_VERSION="2016.4"
source $XILINX_ROOT/Vivado/${VIV_VERSION}/settings64.sh

echo "****************************************************************************************"
echo "Setup SNAP Build Environment ..."
echo "****************************************************************************************"

export SIMULATOR=ncsim
export SDRAM_USED=FALSE
export SNAP_ROOT=`pwd`/snap
export PSLSE_ROOT=`pwd`/pslse
export ACTION_ROOT=$SNAP_ROOT/hardware/action_examples/hls_blowfish/vhdl
export FPGACARD=KU3
export PSL_DCP=/afs/bb/proj/fpga/framework/cards/KU3/rblack_20170117/b_route_design.dcp

source /afs/bb/proj/fpga/framework/ibm_settings_for_snap
source $SNAP_ROOT/hardware/snap_settings

make -C snap clean
make -C snap/hardware config model

echo "****************************************************************************************"
echo "Now try BLOWFISH"
echo "****************************************************************************************"

pushd .

cd $SNAP_ROOT/hardware/sim
export SNAP_TRACE=0xf
./run_sim -explore -app examples/snap_blowfish -arg "-T"

dd if=/dev/urandom of=$SNAP_ROOT/software/input.bin count=16 bs=64
dd if=/dev/urandom of=$SNAP_ROOT/software/key.bin count=1 bs=16

# Encrypt
./run_sim -explore -app examples/snap_blowfish \
	-arg "-k $SNAP_ROOT/software/key.bin -i $SNAP_ROOT/software/input.bin -o $SNAP_ROOT/software/encrypted.bin"

# Decrypt
./run_sim -explore -app examples/snap_blowfish \
	-arg "-k $SNAP_ROOT/software/key.bin -d -i $SNAP_ROOT/software/encrypted.bin -o $SNAP_ROOT/software/decrypted.bin"

diff $SNAP_ROOT/software/input.bin $SNAP_ROOT/software/decrypted.bin
if [ $? -ne 0 ]; then
	echo "ERR: Data does not match!"
	hexdump $SNAP_ROOT/software/input.bin > $SNAP_ROOT/software/input.hex
	hexdump $SNAP_ROOT/software/decrypted.bin > $SNAP_ROOT/software/decrypted.hex
	meld $SNAP_ROOT/software/input.hex $SNAP_ROOT/software/decrypted.hex &
	exit 1
else
	echo "SUCCESS: Data matches decyphered data matches the original!!!!"
fi

popd
exit 0
