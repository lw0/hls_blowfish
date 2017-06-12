#ifndef __ACTION_BLOWFISH_H__
#define __ACTION_BLOWFISH_H__

/*
 * TODO: licensing, implemented according to IBM snap examples
 */
#include <snap_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLOWFISH_ACTION_TYPE 0x00000108

#ifndef CACHELINE_BYTES
#define CACHELINE_BYTES 128
#endif
unsigned int * g_out_ptr; //todo ?

// Blowfish Configuration PATTERN.
// This must match with DATA structure in hls_blowfish/kernel.cpp
typedef struct blowfish_job {
    struct snap_addr input_data;
    struct snap_addr output_data;
    uint32_t data_mode; // 0 for encryption and 1 (or anything not 0) for decryption
    uint32_t data_length; // should be padded and 128 byte aligned
} blowfish_job_t;

typedef struct blowfish_out {
    uint32_t length;
    char * data;
} blowfish_out_t;

int blowfish(char * input_data, unsigned int input_length, unsigned int mode, blowfish_out_t * output);

#ifdef __cplusplus
}
#endif
#endif	/* __ACTION_BFS_H__ */
