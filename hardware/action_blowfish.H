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

#include "hls_snap.H"
#include <action_blowfish.h> /*BFS job definition*/

#define BF_BLOCKBITS 64
#define BF_BLOCK_BADR_BITS 3
#define BF_BLOCK_BADR_MASK 0x7
#define BF_HBLOCKBITS 32
#define BF_HBLOCK_BADR_BITS 2
#define BF_HBLOCK_BADR_MASK 0x3
#define BF_KEY_HBLOCK_MIN 1
#define BF_KEY_HBLOCK_MAX 16
#define BF_SENTRYBITS 8

#define BF_BLOCKSPERLINE (MEMDW/BF_BLOCKBITS)

typedef ap_uint<BF_BLOCKBITS> bf_block_t;
typedef ap_uint<BF_HBLOCKBITS> bf_halfBlock_t;
typedef ap_uint<BF_SENTRYBITS> bf_sEntry_t;

typedef bf_halfBlock_t bf_S_t[4][1<<BF_SENTRYBITS];
typedef bf_halfBlock_t bf_P_t[18];

//---------------------------------------------------------------------
typedef struct {
	CONTROL Control;	/*  16 bytes */
	blowfish_job_t Data;	/* 108 bytes */
	uint8_t padding[SNAP_HLS_JOBSIZE - sizeof(blowfish_job_t)];
} action_reg;

#endif  /* __ACTION_HLS_BLOWFISH_H__ */
