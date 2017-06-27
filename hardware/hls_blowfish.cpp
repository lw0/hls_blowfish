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

#define HW_RELEASE_LEVEL       0x00000013

#include "hls_blowfish_data.hpp"

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
    for (int i = 0; i < 16; i += 2)
    {
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
}

static void bf_decrypt(bf_halfBlock_t & left, bf_halfBlock_t & right)
{
    for (int i = 16; i > 0; i -= 2)
    {
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
}

static void bf_keyInit(snap_membus_t key, snapu8_t keyWords)
{
    for (int i = 0; i < 18; ++i)
    {
        bf_halfBlock_t keyWord = key((i%keyWords)*32,(i%keyWords)*32+31);
        g_P[i] = c_initP[i] ^ keyWord;
    }
    for (int n = 0; n < 4; ++n)
    {
        for (int i = 0; i < 256; ++i)
        {
            g_S[n][i] = c_initS[n][i];
        }
    }
    bf_halfBlock_t left = 0, right = 0;
    for (int i = 0; i < 18; i += 2)
    {
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
    
	// cast line to keyword granularity, initialize g_S, and g_P arrays
	bf_keyInit(keyLine, keyWords);

	return SNAP_RETC_SUCCESS;
}

static snapu32_t action_endecrypt(snap_membus_t * hostMem_in, snapu64_t inAddr,
                            snap_membus_t * hostMem_out, snapu64_t outAddr,
                            snapu32_t dataBytes, snap_bool_t decrypt)
{
    snapu64_t inLineAddr = inAddr >> ADDR_RIGHT_SHIFT;
    snapu64_t outLineAddr = outAddr >> ADDR_RIGHT_SHIFT;
    snapu32_t dataBlocks = dataBytes >> BF_BLOCK_BADR_BITS;

#if 0 /* FIXME check if the condition is correct */
    if ((dataBlocks & BF_BLOCK_BADR_MASK) != 0) // check blockwidth alignment
    {
	fprintf(stderr, "ERR: dataBytes=%d dataBlocks=%d BF_BLOCKSPERLINE=%d non correctly aligned!\n",
		(int)dataBytes, (int)dataBlocks, BF_BLOCKSPERLINE);
        return SNAP_RETC_FAILURE;
    }
#endif

    snapu32_t lineCount = dataBlocks / BF_BLOCKSPERLINE;
    fprintf(stderr, "Processing lineCount=%d ...\n", (int)lineCount);

    for (snapu32_t lineOffset = 0; lineOffset < lineCount; ++lineOffset)
    {
	fprintf(stderr, "Processing lineOffset=%d ...\n", (int)lineOffset);

        // fetch next line
        snap_membus_t line = hostMem_in[inLineAddr + lineOffset];

        // determine number of valid blocks in line
        snapu8_t blockCount = dataBlocks - (lineOffset * BF_BLOCKSPERLINE);
        if (blockCount > BF_BLOCKSPERLINE)
        {
            blockCount = BF_BLOCKSPERLINE;
        }

        // blockwise processing
        for (snapu8_t blockOffset = 0; blockOffset < blockCount; ++blockOffset)
        {
            bf_halfBlock_t left = line(blockOffset + BF_HBLOCKBITS,
				       blockOffset + BF_BLOCKBITS-1);
            bf_halfBlock_t right = line(blockOffset,
					blockOffset + BF_HBLOCKBITS-1);

            if (decrypt)
                bf_decrypt(left, right);
            else
                bf_encrypt(left, right);

            line(blockOffset+BF_HBLOCKBITS, blockOffset + BF_BLOCKBITS-1) = left;
            line(blockOffset, blockOffset + BF_HBLOCKBITS-1) = right;
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
	/* snapu64_t inAddrSwp; */
	/* snapu64_t outAddrSwp; */
	snapu32_t byteCount;
	snapu32_t mode;

	//== Parameters fetched in memory ==
	//==================================

	// byte address received need to be aligned with port width
	inAddr  = action_reg->Data.input_data.addr;
	outAddr  = action_reg->Data.output_data.addr;
	byteCount = action_reg->Data.data_length;
	mode = action_reg->Data.mode;

	//TODO debug:
	/* inAddrSwp = ((inAddr >> 56) & 0x00000000000000ff) | */
	/*             ((inAddr >> 48) & 0x000000000000ff00) | */
	/*             ((inAddr >> 40) & 0x0000000000ff0000) | */
	/*             ((inAddr >> 32) & 0x00000000ff000000) | */
	/*             ((inAddr >> 24) & 0x000000ff00000000) | */
	/*             ((inAddr >> 16) & 0x0000ff0000000000) | */
	/*             ((inAddr >>  8) & 0x00ff000000000000) | */
	/*             ((inAddr >>  0) & 0xff00000000000000); */
	/* outAddrSwp = ((outAddr >> 56) & 0x00000000000000ff) | */
	/*              ((outAddr >> 48) & 0x000000000000ff00) | */
	/*              ((outAddr >> 40) & 0x0000000000ff0000) | */
	/*              ((outAddr >> 32) & 0x00000000ff000000) | */
	/*              ((outAddr >> 24) & 0x000000ff00000000) | */
	/*              ((outAddr >> 16) & 0x0000ff0000000000) | */
	/*              ((outAddr >>  8) & 0x00ff000000000000) | */
	/*              ((outAddr >>  0) & 0xff00000000000000); */

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
	/* (din_gmem + (inAddrSwp >>ADDR_RIGHT_SHIFT))[0] =    0x3333333333333333333333333333333333333333333333333333333333333333; */
	/* (din_gmem + (outAddrSwp >>ADDR_RIGHT_SHIFT))[0] =   0x4444444444444444444444444444444444444444444444444444444444444444; */
	/* (dout_gmem + (inAddr >>ADDR_RIGHT_SHIFT))[0] =      0x5555555555555555555555555555555555555555555555555555555555555555; */
	/* (dout_gmem + (outAddr >>ADDR_RIGHT_SHIFT))[0] =     0x6666666666666666666666666666666666666666666666666666666666666666; */
	/* (dout_gmem + (inAddrSwp >>ADDR_RIGHT_SHIFT))[0] =   0x7777777777777777777777777777777777777777777777777777777777777777; */
	/* (dout_gmem + (outAddrSwp >>ADDR_RIGHT_SHIFT))[0] =  0x8888888888888888888888888888888888888888888888888888888888888888; */

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
#pragma HLS INTERFACE s_axilite port=din_gmem bundle=ctrl_reg 		offset=0x030
#pragma HLS INTERFACE s_axilite port=dout_gmem bundle=ctrl_reg 		offset=0x040

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

    act_reg.Control.flags = 0x0;
    hls_action(din_gmem, dout_gmem, &act_reg, &act_config);
    fprintf(stderr, "ACTION_TYPE:   %08x\nRELEASE_LEVEL: %08x\nRETC:          %04x\n",
	    (unsigned int)act_config.action_type,
	    (unsigned int)act_config.release_level,
	    (unsigned int)act_reg.Control.Retc);

    *(uint64_t *)(void *)&din_gmem[0] = 0x0706050403020100ull; // key 8 Byte at 0x0
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
    const uint8_t *plain = (uint8_t *)(void *)&din_gmem[2];
    for (unsigned int i = 0; i < act_reg.Data.data_length; ++i) {
	    printf("%02x ", plain[i]);
    }

    printf("\ncipher: ");
    const uint8_t *cypher = (uint8_t *)(void *)&dout_gmem[4];
    for (unsigned int i = 0; i < act_reg.Data.data_length; ++i) {
	    printf("%02x ", cypher[i]);
    }

    printf("\ndin:\n");
    for (unsigned int i = 0; i < ARRAY_SIZE(din_gmem); i++)
	    if (din_gmem[i] != 0)
		    cout << setw(4)  << i * sizeof(snap_membus_t) << ": "
			 << setw(32) << hex << din_gmem[i]
			 << endl;

    printf("\ndout:\n");
    for (unsigned int i = 0; i < ARRAY_SIZE(dout_gmem); i++)
	    if (dout_gmem[i] != 0)
		    cout << setw(4)  << i * sizeof(snap_membus_t) << ": "
			 << setw(32) << hex << dout_gmem[i]
			 << endl;

    /* FIXME Memcmp() needed to check the correctness of the result */
}

#endif /* NO_SYNTH */
