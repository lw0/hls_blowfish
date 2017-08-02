#ifndef __ACTION_HLS_BLOWFISH_H__
#define __ACTION_HLS_BLOWFISH_H__

/*
 * Copyright 2017, International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hls_snap.H>
#include <action_blowfish.h> /*BFS job definition*/

// BF_INSTANCES: # of parallel bf_encrypt/bf_decrypt instances to be synthesized
#define BF_INST_W 4
#define BF_INST (1 << BF_INST_W)

// 
#define BF_BLOCKBITS 64
#define BF_BLOCK_BADR_BITS 3
#define BF_BLOCK_BADR_MASK 0x7
#define BF_HBLOCKBITS 32
#define BF_HBLOCK_BADR_BITS 2
#define BF_HBLOCK_BADR_MASK 0x3
#define BF_KEY_HBLOCK_MIN 1
#define BF_KEY_HBLOCK_MAX 16

// S arrays (BF_INST/2 copies of 4 arrays with 256 entries * 32b)
#define BF_S_CPYCNT_W (BF_INST_W - 1)
#define BF_S_CPYCNT (1 << BF_S_CPYCNT_W)
#define BF_S_ARYCNT_W 2
#define BF_S_ARYCNT (1 << BF_S_ARYCNT_W)
#define BF_S_ENTCNT_W 8
#define BF_S_ENTCNT (1 << BF_S_ENTCNT_W)
#define BF_S_DATA_W 32

// P array (18 entries * 32b)
#define BF_P_ENTCNT_W 5
#define BF_P_ENTCNT 18
#define BF_P_DATA_W 32

// blocks per line (block: 8B / 64b; line 128B / 1024b)
#define BF_BPL_W 4
#define BF_BPL (1 << BF_BPL_W)

typedef ap_uint<BF_BLOCKBITS> bf_block_t;
typedef ap_uint<BF_HBLOCKBITS> bf_halfBlock_t;

typedef ap_uint<BF_S_DATA_W> bf_S_t[BF_S_CPYCNT][BF_S_ARYCNT][BF_S_ENTCNT];
typedef ap_uint<BF_S_DATA_W> bf_initS_t[BF_S_ARYCNT][BF_S_ENTCNT];
typedef ap_uint<BF_S_CPYCNT_W + 1> bf_SiC_t;
typedef ap_uint<BF_S_ARYCNT_W + 1> bf_SiA_t;
typedef ap_uint<BF_S_ENTCNT_W + 1> bf_SiE_t;

typedef ap_uint<BF_P_DATA_W> bf_P_t[BF_P_ENTCNT];
typedef ap_uint<BF_P_ENTCNT_W> bf_PiE_t;

typedef ap_uint<BF_BPL_W + 1> bf_uiBpL_t;

typedef ap_uint<5> bf_iteration_t;

//---------------------------------------------------------------------
typedef struct {
    CONTROL Control;	/*  16 bytes */
    blowfish_job_t Data;	/* 108 bytes */
    uint8_t padding[SNAP_HLS_JOBSIZE - sizeof(blowfish_job_t)];
} action_reg;

#endif  /* __ACTION_HLS_BLOWFISH_H__ */
