#!/bin/bash
#
# Copyright 2017 International Business Machines
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
make -C snap/hardware config image

