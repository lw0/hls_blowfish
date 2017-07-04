/**
 * FIXME Remarks
 *  - Let us try to reduce the loop counts to the absolutely minimum size,
 *    that has the potential to reduce the logic size and improve the
 *    timing.
 *  - Next step would be to check if we managed to get some parallelism
 *    in data processing. We might want to add some pragmals to improve
 *    and/or control this.
 *  - Timing seems ok so far.
 */

#include <string.h>
#include <iostream>
#include <hls_stream.h>
#include "ap_int.h"
#include "action_blowfish.H"
#include "hls_blowfish_data.hpp"

#define HW_RELEASE_LEVEL       0x00000013

using namespace std;

static bf_P_t g_P;
static bf_S_t g_S;

static void print_line(const char *msg, snap_membus_t line)
{
#ifdef NO_SYNTH
	cout << msg << ": " << setw(32) << hex << line
	     << endl;
#endif
}

static bf_halfBlock_t bf_lineToHBlock(const snap_membus_t & line, unsigned firstByte)
{
	bf_halfBlock_t h = 0;
	// big endian
	h |= line.range(firstByte*8 +  7, firstByte*8 +  0) << 24;
	h |= line.range(firstByte*8 + 15, firstByte*8 +  8) << 16;
	h |= line.range(firstByte*8 + 23, firstByte*8 + 16) <<  8;
	h |= line.range(firstByte*8 + 31, firstByte*8 + 24) <<  0;

	// little endian
//	h |= line.range(firstByte*8 +  7, firstByte*8 +  0) <<  0;
//	h |= line.range(firstByte*8 + 15, firstByte*8 +  8) <<  8;
//	h |= line.range(firstByte*8 + 23, firstByte*8 + 16) << 16;
//	h |= line.range(firstByte*8 + 31, firstByte*8 + 24) << 24;

	return h;
}

static void bf_hBlockToLine(snap_membus_t & line, unsigned firstByte, bf_halfBlock_t h)
{
	// big endian
//	line.range(firstByte*8 +  7, firstByte*8 +  0) = (h >> 24) & 0xff;
//	line.range(firstByte*8 + 15, firstByte*8 +  8) = (h >> 16) & 0xff;
//	line.range(firstByte*8 + 23, firstByte*8 + 16) = (h >>  8) & 0xff;
//	line.range(firstByte*8 + 31, firstByte*8 + 24) = (h >>  0) & 0xff;

	// little endian
	line.range(firstByte*8 +  7, firstByte*8 +  0) = (h >>  0) & 0xff;
	line.range(firstByte*8 + 15, firstByte*8 +  8) = (h >>  8) & 0xff;
	line.range(firstByte*8 + 23, firstByte*8 + 16) = (h >> 16) & 0xff;
	line.range(firstByte*8 + 31, firstByte*8 + 24) = (h >> 24) & 0xff;
}


static void bf_lineToBlock(const snap_membus_t & line, unsigned firstByte, bf_halfBlock_t & l, bf_halfBlock_t & r)
{
	l = 0;
	r = 0;
	// big endian
	l |= line.range(firstByte*8 +  7, firstByte*8 +  0) << 24;
	l |= line.range(firstByte*8 + 15, firstByte*8 +  8) << 16;
	l |= line.range(firstByte*8 + 23, firstByte*8 + 16) <<  8;
	l |= line.range(firstByte*8 + 31, firstByte*8 + 24) <<  0;

	r |= line.range(firstByte*8 + 39, firstByte*8 + 32) << 24;
	r |= line.range(firstByte*8 + 47, firstByte*8 + 40) << 16;
	r |= line.range(firstByte*8 + 55, firstByte*8 + 48) <<  8;
	r |= line.range(firstByte*8 + 63, firstByte*8 + 56) <<  0;

	// little endian
//	l |= line.range(firstByte*8 +  7, firstByte*8 +  0) <<  0;
//	l |= line.range(firstByte*8 + 15, firstByte*8 +  8) <<  8;
//	l |= line.range(firstByte*8 + 23, firstByte*8 + 16) << 16;
//	l |= line.range(firstByte*8 + 31, firstByte*8 + 24) << 24;
//
//	r |= line.range(firstByte*8 + 39, firstByte*8 + 32) <<  0;
//	r |= line.range(firstByte*8 + 47, firstByte*8 + 40) <<  8;
//	r |= line.range(firstByte*8 + 55, firstByte*8 + 48) << 16;
//	r |= line.range(firstByte*8 + 63, firstByte*8 + 56) << 24;
}

static void bf_blockToLine(snap_membus_t & line, unsigned firstByte, bf_halfBlock_t l, bf_halfBlock_t r)
{
	// big endian
	line.range(firstByte*8 +  7, firstByte*8 +  0) = (l >> 24) & 0xff;
	line.range(firstByte*8 + 15, firstByte*8 +  8) = (l >> 16) & 0xff;
	line.range(firstByte*8 + 23, firstByte*8 + 16) = (l >>  8) & 0xff;
	line.range(firstByte*8 + 31, firstByte*8 + 24) = (l >>  0) & 0xff;

	line.range(firstByte*8 + 39, firstByte*8 + 32) = (r >> 24) & 0xff;
	line.range(firstByte*8 + 47, firstByte*8 + 40) = (r >> 16) & 0xff;
	line.range(firstByte*8 + 55, firstByte*8 + 48) = (r >>  8) & 0xff;
	line.range(firstByte*8 + 63, firstByte*8 + 56) = (r >>  0) & 0xff;

	// little endian
//	line.range(firstByte*8 +  7, firstByte*8 +  0) = (l >>  0) & 0xff;
//	line.range(firstByte*8 + 15, firstByte*8 +  8) = (l >>  8) & 0xff;
//	line.range(firstByte*8 + 23, firstByte*8 + 16) = (l >> 16) & 0xff;
//	line.range(firstByte*8 + 31, firstByte*8 + 24) = (l >> 24) & 0xff;
//
//	line.range(firstByte*8 + 39, firstByte*8 + 32) = (r >>  0) & 0xff;
//	line.range(firstByte*8 + 47, firstByte*8 + 40) = (r >>  8) & 0xff;
//	line.range(firstByte*8 + 55, firstByte*8 + 48) = (r >> 16) & 0xff;
//	line.range(firstByte*8 + 63, firstByte*8 + 56) = (r >> 24) & 0xff;
}

static bf_halfBlock_t bf_f(bf_halfBlock_t h)
{
    bf_sEntry_t a = (bf_sEntry_t)(h >> 24),
                b = (bf_sEntry_t)(h >> 16),
                c = (bf_sEntry_t)(h >> 8),
                d = (bf_sEntry_t) h;
    return ((g_S[0][a] + g_S[1][b]) ^ g_S[2][c]) + g_S[3][d];
}

static void bf_encrypt(bf_halfBlock_t & left, bf_halfBlock_t & right)
{
	printf("bf_encrypt(0x%08x, 0x%08x)", *((uint32_t *)(void *)&left), *((uint32_t *)(void *)&right));

BF_ENCRYPT:
    for (int i = 0; i < 16; i += 2) {
#pragma HLS_UNROLL factor=16
        left ^= g_P[i];
        right ^= bf_f(left);
        right ^= g_P[i+1];
        left ^= bf_f(right);
    }
    left ^= g_P[16];
    right ^= g_P[17];

    // swap left, right
    bf_halfBlock_t tmp = left;
    left = right;
    right = tmp;
    printf(" -> 0x%08x, 0x%08x\n", *((uint32_t *)(void *)&left), *((uint32_t *)(void *)&right));
}

static void bf_decrypt(bf_halfBlock_t & left, bf_halfBlock_t & right)
{
	printf("bf_decrypt(0x%08x, 0x%08x)", *((uint32_t *)(void *)&left), *((uint32_t *)(void *)&right));

BF_DECRYPT:
	for (int i = 16; i > 0; i -= 2) {
#pragma HLS_UNROLL factor=16
        left ^= g_P[i+1];
        right ^= bf_f(left);
        right ^= g_P[i];
        left ^= bf_f(right);
    }
    left ^= g_P[1];
    right ^= g_P[0];

    // swap left, right
    bf_halfBlock_t tmp = left;
    left = right;
    right = tmp;
    printf(" -> 0x%08x, 0x%08x\n", *((uint32_t *)(void *)&left), *((uint32_t *)(void *)&right));
}

static void bf_keyInit(bf_halfBlock_t key[18])
{
	printf("bf_keyInit() <- \n");
    for (int i = 0; i < 18; ++i) {
        g_P[i] = c_initP[i] ^ key[i];
    }
    for (int n = 0; n < 4; ++n) {
        for (int i = 0; i < 256; ++i) {
            g_S[n][i] = c_initS[n][i];
        }
    }
    bf_halfBlock_t left = 0, right = 0;
    for (int i = 0; i < 18; i += 2) {
        bf_encrypt(left, right);
        g_P[i] = left;
        g_P[i+1] = right;
    }
    for (int n = 0; n < 4; ++n)
    {
        for (int i = 0; i < 256; i += 2)
        {
            bf_encrypt(left, right);
            g_S[n][i] = left;
            g_S[n][i+1] = right;
        }
    }
}

static snapu32_t action_setkey(snap_membus_t * hostMem_in,
			       snapu64_t keyAddr, snapu32_t keyBytes)
{
	snapu64_t keyLineAddr = keyAddr >> ADDR_RIGHT_SHIFT;
	snapu8_t keyWords = keyBytes >> BF_HBLOCK_BADR_BITS;

	if ((keyBytes & BF_HBLOCK_BADR_MASK) != 0 || // check keyword (half blockwidth) alignment
	    BF_KEY_HBLOCK_MAX < keyWords ||
	    keyWords < BF_KEY_HBLOCK_MIN) { // check keyword count
		return SNAP_RETC_FAILURE;
	}

	// key fits in a single line, fetch it
	snap_membus_t keyLine = hostMem_in[keyLineAddr];
    
	bf_halfBlock_t key[18];
	for (int i = 0; i < 18; ++i)
	{
		key[i] = bf_lineToHBlock(keyLine, (i % keyWords.to_int()) * 4);
		printf("%d : 0x%08x\n", i, *((uint32_t *)(void *)&key[i]));
	}

	// initialize g_S, and g_P arrays
	bf_keyInit(key);

	return SNAP_RETC_SUCCESS;
}

static snapu32_t action_endecrypt(snap_membus_t * hostMem_in, snapu64_t inAddr,
                            snap_membus_t * hostMem_out, snapu64_t outAddr,
                            snapu32_t dataBytes, snap_bool_t decrypt)
{
    snapu64_t inLineAddr = inAddr >> ADDR_RIGHT_SHIFT;
    snapu64_t outLineAddr = outAddr >> ADDR_RIGHT_SHIFT;
    snapu32_t dataBlocks = dataBytes >> BF_BLOCK_BADR_BITS;

    /* FIXME check if the condition is correct */
    if ((dataBytes & BF_BLOCK_BADR_MASK) != 0) // check blockwidth alignment
    {
	fprintf(stderr, "ERR: dataBytes=%d dataBlocks=%d BF_BLOCKSPERLINE=%d non correctly aligned!\n",
		(int)dataBytes, (int)dataBlocks, BF_BLOCKSPERLINE);
        return SNAP_RETC_FAILURE;
    }

    snapu32_t lineCount = dataBlocks / BF_BLOCKSPERLINE;
    fprintf(stderr, "Processing lineCount=%d ...\n", (int)lineCount);

    for (snapu32_t lineOffset = 0; lineOffset < lineCount; ++lineOffset)
    {
	fprintf(stderr, "Processing lineOffset=%d ...\n", (int)lineOffset);

        // fetch next line
        snap_membus_t line = hostMem_in[inLineAddr + lineOffset];

        // determine number of valid blocks in line
        snapu8_t blocksDone = (lineOffset * BF_BLOCKSPERLINE);
        snapu8_t blockCount = dataBlocks - blocksDone;
        if (blockCount > BF_BLOCKSPERLINE)
        {
            blockCount = BF_BLOCKSPERLINE;
        }

        // blockwise processing
        for (snapu8_t blockOffset = 0; blockOffset < blockCount; ++blockOffset)
        {
        	snapu16_t blockBitOffset = blockOffset * BF_BLOCKBITS;
        	//bf_halfBlock_t right =  line.range(blockBitOffset + BF_BLOCKBITS -1, blockBitOffset + BF_HBLOCKBITS);
			//bf_halfBlock_t left = line.range(blockBitOffset + BF_HBLOCKBITS-1, blockBitOffset + 0);
            bf_halfBlock_t left, right;
            bf_lineToBlock(line, blockOffset * 8, left, right);

            //left = bf_bswap(left);
            //right = bf_bswap(right);

            if (decrypt)
                bf_decrypt(left, right);
            else
                bf_encrypt(left, right);

            //left = bf_bswap(left);
			//right = bf_bswap(right);

            bf_blockToLine(line, blockOffset * 8, left, right);
            //line.range(blockBitOffset + BF_BLOCKBITS -1, blockBitOffset + BF_HBLOCKBITS) = right;
            //line.range(blockBitOffset + BF_HBLOCKBITS-1, blockBitOffset + 0) = left;
        }

        // write processed line
        print_line("write", line);
        hostMem_out[outLineAddr + lineOffset] = line;
    }

    return SNAP_RETC_SUCCESS;
}

static snapu32_t process_action(snap_membus_t * din_gmem,
                                snap_membus_t * dout_gmem,
                                action_reg * action_reg)
{
	snapu64_t inAddr;
	snapu64_t outAddr;
	snapu32_t byteCount;
	snapu32_t mode;

	//== Parameters fetched in memory ==
	//==================================

	// byte address received need to be aligned with port width
	inAddr  = action_reg->Data.input_data.addr;
	outAddr  = action_reg->Data.output_data.addr;
	byteCount = action_reg->Data.data_length;
	mode = action_reg->Data.mode;

	snapu32_t retc = SNAP_RETC_SUCCESS;
	switch (mode) {
	case MODE_SET_KEY:
		retc = action_setkey(din_gmem, inAddr, byteCount);
		break;
	case MODE_ENCRYPT:
		retc = action_endecrypt(din_gmem, inAddr, dout_gmem, outAddr,
					byteCount, 0);
		break;
	case MODE_DECRYPT:
		retc = action_endecrypt(din_gmem, inAddr, dout_gmem, outAddr,
					byteCount, 1);
		break;
	default:
		break;
	}

	/* snap_membus_t buffer[16]; */
	/* memcpy(buffer, (snap_membus_t *)(din_gmem + (inAddr >> ADDR_RIGHT_SHIFT)), 64); */
	/* memcpy((snap_membus_t *)(dout_gmem + (outAddr >> ADDR_RIGHT_SHIFT)), buffer, 64); */

	/* (din_gmem + (inAddr >>ADDR_RIGHT_SHIFT))[0] =       0x1111111111111111111111111111111111111111111111111111111111111111; */
	/* (din_gmem + (outAddr >>ADDR_RIGHT_SHIFT))[0] =      0x2222222222222222222222222222222222222222222222222222222222222222; */
	/* (dout_gmem + (inAddr >>ADDR_RIGHT_SHIFT))[0] =      0x5555555555555555555555555555555555555555555555555555555555555555; */
	/* (dout_gmem + (outAddr >>ADDR_RIGHT_SHIFT))[0] =     0x6666666666666666666666666666666666666666666666666666666666666666; */

	return retc;
}


//--------------------------------------------------------------------------------------------
//--- MAIN PROGRAM ---------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
// This example doesn't use FPGA DDR.
// Need to set Environment Variable "SDRAM_USED=FALSE" before compilation.
void hls_action(snap_membus_t  *din_gmem, snap_membus_t  *dout_gmem,
		action_reg *action_reg, action_RO_config_reg *Action_Config)
{
    // Host Memory AXI Interface
#pragma HLS INTERFACE m_axi port=din_gmem bundle=host_mem offset=slave depth=512
#pragma HLS INTERFACE m_axi port=dout_gmem bundle=host_mem offset=slave depth=512
#pragma HLS INTERFACE s_axilite port=din_gmem bundle=ctrl_reg offset=0x030
#pragma HLS INTERFACE s_axilite port=dout_gmem bundle=ctrl_reg offset=0x040

    // Host Memory AXI Lite Master Interface
#pragma HLS DATA_PACK variable=Action_Config
#pragma HLS INTERFACE s_axilite port=Action_Config bundle=ctrl_reg	offset=0x010
#pragma HLS DATA_PACK variable=action_reg
#pragma HLS INTERFACE s_axilite port=action_reg bundle=ctrl_reg	offset=0x100
#pragma HLS INTERFACE s_axilite port=return bundle=ctrl_reg

	/* Required Action Type Detection */
	switch (action_reg->Control.flags) {
        case 0:
		Action_Config->action_type = (snapu32_t)BLOWFISH_ACTION_TYPE;
		Action_Config->release_level = (snapu32_t)HW_RELEASE_LEVEL;
		action_reg->Control.Retc = (snapu32_t)0xe00f;
		break;
        default:
		action_reg->Control.Retc = process_action(din_gmem, dout_gmem, action_reg);
		break;
	}
}

//-----------------------------------------------------------------------------
//--- TESTBENCH ---------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef NO_SYNTH

int main()
{
    static snap_membus_t din_gmem[1024];
    static snap_membus_t dout_gmem[1024];
    action_reg act_reg;
    action_RO_config_reg act_config;

    /* FIXME let us try 64 bytes to get a full line as starter... */
    static const uint8_t ptext[] = {
    		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, /*  8 bytes */
		    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, /* 16 bytes */
		    0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, /* 24 bytes */
		    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, /* 32 bytes */
		    0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, /* 40 bytes */
		    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, /* 48 bytes */
		    0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, /* 56 bytes */
			0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, /* 64 bytes */
    };
    static const uint8_t key[] = {
    		0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, /*  8 bytes */
    };

    act_reg.Control.flags = 0x0;
    hls_action(din_gmem, dout_gmem, &act_reg, &act_config);
    fprintf(stderr, "ACTION_TYPE:   %08x\nRELEASE_LEVEL: %08x\nRETC:          %04x\n",
	    (unsigned int)act_config.action_type,
	    (unsigned int)act_config.release_level,
	    (unsigned int)act_reg.Control.Retc);

    memcpy((uint8_t *)(void *)&din_gmem[0], key, sizeof(key)); // key 8 Byte at 0x0
    memcpy((uint8_t *)(void *)&din_gmem[2], ptext, sizeof(ptext)); // plaintext 16 Byte at 0x80

    /* FIXME Why multiple calls? We could pass the key with the
       encrypt/decrypt request to avoid hardware calls. But maybe
       setting it once and streaming data is a good idea too. */

    fprintf(stderr, "// MODE_SET_KEY ciphertext 16 Byte at 0x100\n");
    act_reg.Control.flags = 0x1;
    act_reg.Data.input_data.addr = 0;
    act_reg.Data.data_length = 8;
    act_reg.Data.mode = MODE_SET_KEY;

    hls_action(din_gmem, dout_gmem, &act_reg, &act_config);

    fprintf(stderr, "// MODE_ENCRYPT data ...\n");
    act_reg.Control.flags = 0x1;
    act_reg.Data.input_data.addr = 2 * sizeof(snap_membus_t);
    act_reg.Data.output_data.addr = 4 * sizeof(snap_membus_t);
    act_reg.Data.data_length = sizeof(ptext);
    act_reg.Data.mode = MODE_ENCRYPT;

    hls_action(din_gmem, dout_gmem, &act_reg, &act_config);
    
    printf("plain:  ");
    const uint8_t *plainE = (uint8_t *)(void *)&din_gmem[2];
    for (unsigned int i = 0; i < act_reg.Data.data_length; ++i) {
	    printf("%02x ", plainE[i]);
    }

    printf("\ncipher: ");
    const uint8_t *cipherE = (uint8_t *)(void *)&dout_gmem[4];
    for (unsigned int i = 0; i < act_reg.Data.data_length; ++i) {
	    printf("%02x ", cipherE[i]);
    }

    memcpy((uint8_t *)(void *)&din_gmem[4], (uint8_t *)(void *)&dout_gmem[4], sizeof(ptext));

    fprintf(stderr, "// MODE_DECRYPT data ...\n");
	act_reg.Control.flags = 0x1;
	act_reg.Data.input_data.addr = 4 * sizeof(snap_membus_t);
	act_reg.Data.output_data.addr = 6 * sizeof(snap_membus_t);
	act_reg.Data.data_length = sizeof(ptext);
	act_reg.Data.mode = MODE_DECRYPT;

	hls_action(din_gmem, dout_gmem, &act_reg, &act_config);

	printf("cipher:  ");
	const uint8_t *cipherD = (uint8_t *)(void *)&din_gmem[4];
	for (unsigned int i = 0; i < act_reg.Data.data_length; ++i) {
		printf("%02x ", cipherD[i]);
	}

	printf("\nplain: ");
	const uint8_t *plainD = (uint8_t *)(void *)&dout_gmem[6];
	for (unsigned int i = 0; i < act_reg.Data.data_length; ++i) {
		printf("%02x ", plainD[i]);
	}
	printf("\n");

    /* FIXME Memcmp() needed to check the correctness of the result */
	if (memcmp(plainE, plainD, sizeof(ptext)) != 0) {
		printf("Well, something is wrong, please investigate!\n");
		return -1;
	}

	printf("Super the data is the same!\n");
	return 0;
}

#endif /* NO_SYNTH */
