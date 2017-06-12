#include <string.h>
#include "ap_int.h"
#include <hls_stream.h>
#include "action_blowfish.H"

#define HW_RELEASE_LEVEL       0x00000013

#include "hls_blowfish_data.hpp"

bf_P_t g_P;
bf_S_t g_S;

bf_halfBlock_t bf_f(bf_halfBlock_t h)
{
    bf_sEntry_t a = (bf_sEntry_t)(h >> 24),
                b = (bf_sEntry_t)(h >> 16),
                c = (bf_sEntry_t)(h >> 8),
                d = (bf_sEntry_t) h;
    return ((g_S[0][a] + g_S[1][b]) ^ g_S[2][c]) + g_S[3][d];
}

void bf_encrypt(bf_halfBlock_t & left, bf_halfBlock_t & right)
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

void bf_decrypt(bf_halfBlock_t & left, bf_halfBlock_t & right)
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

bool bf_keyInit(bf_halfBlock_t * key, snapu8_t keyWords)
{
    for (int i = 0; i < 18; ++i)
    {
        g_P[i] = c_initP[i] ^ key[i%keyWords];
    }
    for (int n=0 ; n<4 ; ++n)
    {
        for (int i=0 ; i<256; ++i)
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
    for (int n=0 ; n<4 ; ++n)
    {
        for (int i=0 ; i<256; i+=2)
        {
            bf_encrypt(left, right);
            g_S[n][i] = left;
            g_S[n][i+1] = right;
        }
    }
}

snapu32_t action_setkey(snap_membus_t * hostMem_in, snapu64_t keyAddr, snapu32_t keyBytes)
{
    snapu64_t keyLineAddr = keyAddr >> ADDR_RIGHT_SHIFT;
    snapu8_t keyWords = keyBytes >> BF_HBLOCK_BADR_BITS;
    if ((keyBytes & BF_HBLOCK_BADR_MASK) != 0 || // check keyword (half blockwidth) alignment
        keyWords < BF_KEY_HBLOCK_MIN || keyWords > BF_KEY_HBLOCK_MAX) // check keyword count
    {
        return SNAP_RETC_FAILURE;
    }
    
    // key fits in a single line, fetch it
    snap_membus_t keyLine = *(hostMem_in + keyLineAddr);
    
    // cast line to keyword granularity, initialize g_S, and g_P arrays
    bf_keyInit((bf_halfBlock_t *)(&keyLine), keyWords);

    return SNAP_RETC_SUCCESS;
}

snapu32_t action_endecrypt(snap_membus_t * hostMem_in, snapu64_t inAddr,
                            snap_membus_t * hostMem_out, snapu64_t outAddr,
                            snapu32_t dataBytes, snap_bool_t decrypt)
{
    snapu64_t inLineAddr = inAddr >> ADDR_RIGHT_SHIFT;
    snapu64_t outLineAddr = outAddr >> ADDR_RIGHT_SHIFT;
    snapu32_t dataBlocks = dataBytes >> BF_BLOCK_BADR_BITS;
    if ((dataBlocks & BF_BLOCK_BADR_MASK) != 0) // check blockwidth alignment
    {
        return SNAP_RETC_FAILURE;
    }

    snapu32_t lineCount = dataBlocks / BF_BLOCKSPERLINE;
    for (snapu32_t lineOffset = 0; lineOffset < lineCount; ++lineOffset)
    {
        // fetch next line
        snap_membus_t line = *(hostMem_in + inLineAddr + lineOffset);

        // determine number of valid blocks in line
        snapu8_t blockCount = dataBlocks - (lineOffset * BF_BLOCKSPERLINE);
        if (blockCount > BF_BLOCKSPERLINE)
        {
            blockCount = BF_BLOCKSPERLINE;
        }

        // blockwise processing
        for (snapu8_t blockOffset = 0; blockOffset < blockCount; ++blockOffset)
        {
            bf_halfBlock_t left = line(blockOffset+BF_HBLOCKBITS, blockOffset + BF_BLOCKBITS-1);
            bf_halfBlock_t right = line(blockOffset, blockOffset + BF_HBLOCKBITS-1);

            if (decrypt)
                bf_decrypt(left, right);
            else
                bf_encrypt(left, right);

            line(blockOffset+BF_HBLOCKBITS, blockOffset + BF_BLOCKBITS-1) = left;
            line(blockOffset, blockOffset + BF_HBLOCKBITS-1) = right;
        }

        // write processed line
        *(hostMem_out + outLineAddr + lineOffset) = line;
    }


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

    // VARIABLES
    snapu32_t ReturnCode;

    snapu64_t inAddr;
    snapu64_t outAddr;
    snapu32_t byteCount;
    snapu32_t mode;

    /* Required Action Type Detection */
    switch (action_reg->Control.flags) {
        case 0:
            Action_Config->action_type = (snapu32_t)BLOWFISH_ACTION_TYPE;
            Action_Config->release_level = (snapu32_t)HW_RELEASE_LEVEL;
            action_reg->Control.Retc = (snapu32_t)0xe00f;
            return;
        default:
            break;
    }

    //== Parameters fetched in memory ==
    //==================================

    // byte address received need to be aligned with port width
    inAddr  = action_reg->Data.input_data.addr;
    outAddr  = action_reg->Data.output_data.addr;
    byteCount = action_reg->Data.data_length;
    mode = action_reg->Data.mode;

    printf("Opcode %d (inAddr: %x, outAddr: %x, byteCount: %d)", mode, inAddr, outAddr, byteCount);
    printf("Opcodes: SET_KEY %d, ENCRYPT %d, DECRYPT %d)", MODE_SET_KEY, MODE_ENCRYPT, MODE_DECRYPT);

    snapu32_t retc = SNAP_RETC_SUCCESS;
    switch (mode)
    {
    MODE_SET_KEY:
        retc = action_setkey(din_gmem, inAddr, byteCount);
        break;
    MODE_ENCRYPT:
        retc = action_endecrypt(din_gmem, inAddr, dout_gmem, outAddr, byteCount, 0);
        break;
    MODE_DECRYPT:
        retc = action_endecrypt(din_gmem, inAddr, dout_gmem, outAddr, byteCount, 1);
        break;
    default:
        break;
    }

    action_reg->Control.Retc = retc;
    return;
}


