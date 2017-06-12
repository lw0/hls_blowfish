#include <string.h>
#include "ap_int.h"
#include <hls_stream.h>
#include "action_blowfish.H"

#define HW_RELEASE_LEVEL       0x00000013

#include "hls_blowfish_data.hpp"

bf_P_t g_P;
bf_S_t g_S;

bf_uiHalfBlock_t bf_f(bf_uiHalfBlock_t h)
{
    bf_uiSEntry_t a = (bf_uiSEntry_t)(h >> 24),
                  b = (bf_uiSEntry_t)(h >> 16),
                  c = (bf_uiSEntry_t)(h >> 8),
                  d = (bf_uiSEntry_t) h;
    return ((g_S[0][a] + g_S[1][b]) ^ g_S[2][c]) + g_S[3][d];
}

bf_block_t encrypt(bf_block_t block)
{
    bf_block_t b = block;
    for (int i = 0; i < 16; i += 2)
    {
        b.left ^= g_P[i];
        b.right ^= bf_f(b.left);
        b.right ^= g_P[i+1];
        b.left ^= bf_f(b.right);
    }
    b.left ^= g_P[16];
    b.right ^= g_P[17];

    // swap left, right
    bf_uiHalfBlock_t tmp = b.left;
    b.left = b.right;
    b.right = tmp;
}

bf_block_t decrypt(const bf_block_t block)
{
    bf_block_t b = block;
    for (int i = 16; i > 0; i -= 2)
    {
        b.left ^= P[i+1];
        b.right ^= bf_f(S, b.left);
        b.right ^= P[i];
        b.left ^= bf_f(S, b.right);
    }
    b.left ^= P[1];
    b.right ^= P[0];

    // swap left, right
    bf_uiHalfBlock_t tmp = b.left;
    b.left = b.right;
    b.right = tmp;
}

bool bf_keyInit(bf_uiHalfBlock_t * key, snapu8_t keyWords)
{
    g_P = c_initP;
    g_S = c_initS;
    for (int i = 0; i < 18; ++i)
    {
        g_P ^= key[i%keyWords];
    }
    bf_blockStruct_t b = {.all = 0};
    for (int i = 0; i < 18; i += 2)
    {
        b = encrypt(b);
        g_P[i] = b.left;
        g_P[i+1] = b.right;
    }
    for (int n=0 ; n<4 ; ++n)
    {
        for (int i=0 ; i<256; i+=2)
        {
            b = encrypt(b);
            g_S[n][i] = b.left;
            g_S[n][i+1] = b.right;
        }
    }
}

snapu32_t action_setkey(snap_membus_t * hostMem_in, snapu64_t keyAddr, snapu32_t keyBytes)
{
    snapu8_t keyWords = keyBytes >> BF_KEYW_BADR_BITS;
    if (keyBytes & BF_KEYW_BADR_MASK != 0 || // check keyword (half blockwidth) alignment
        keyWords < BF_KEYW_MIN || keyWords > BF_KEYW_MAX) // check keyword count
    {
        return SNAP_RETC_FAILURE;
    }
    
    // key fits in a single line, fetch it
    snap_membus_t keyLine = *(hostMem_in + (keyAddr >> ADDR_RIGHT_SHIFT));
    
    // cast line to keyword granularity, initialize g_S, and g_P arrays
    bf_keyInit((bf_uiHalfBlock_t *)(&keyLine), keyWords);

    return SNAP_RETC_SUCCESS;
}

snapu32_t action_endecrypt(snap_membus_t * hostMem_in, snapu64_t inAddr,
                            snap_membus_t * hostMem_out, snapu64_t outAddr,
                            snapu32_t dataBytes, snap_bool_t decrypt)
{
    snapu64_t inLineAddr = inAddr >> ADDR_RIGHT_SHIFT;
    snapu64_t outLineAddr = outAddr >> ADDR_RIGHT_SHIFT;
    snapu32_t dataBlocks = dataBytes >> BF_BLOCK_BADR_BITS;
    if (dataBlocks & BF_BLOCK_BADR_MASK != 0) // check blockwidth alignment
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
            bf_block_t block = {.all = line(blockOffset, blockOffset+BF_BLOCKBITS)};
            if (decrypt)
            {
                block = bf_decrypt(block);
            }
            else
            {
                block = bf_encrypt(block);
            }
            line(blockOffset, blockOffset+BF_BLOCKBITS) = block.all;
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

    snapu64_t input_address;
    snapu64_t output_address;
    snapu32_t data_length;
    snapu32_t mode;

    /* Required Action Type Detection */
    switch (action_reg->Control.flags) {
        case 0:
            Action_Config->action_type = (snapu32_t)BFS_ACTION_TYPE;
            Action_Config->release_level = (snapu32_t)HW_RELEASE_LEVEL;
            action_reg->Control.Retc = (snapu32_t)0xe00f;
            return;
        default:
            break;
    }

    //== Parameters fetched in memory ==
    //==================================

    // byte address received need to be aligned with port width
    inAddr  = action_reg->Data.input_data;
    outAddr  = action_reg->Data.output_data;
    dataBytes = action_reg->Data.data_length;
    mode = action_reg->Data.mode;

    snapu32_t retc = SNAP_RETC_FAILURE;
    switch (mode)
    {
    BF_MODE_SETKEY:
        retc = action_setkey(din_gmem, inAddr, dataBytes);
        break;
    BF_MODE_ENCRYPT:
        retc = action_endecrypt(din_gmem, inAddr, dout_gmem, outAddr, dataBytes, 0);
        break;
    BF_MODE_DECRYPT:
        retc = action_endecrypt(din_gmem, inAddr, dout_gmem, outAddr, dataBytes, 1);
        break;
    }

    action_reg->Control.Retc = retc;
    return;
}


