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

#define MODE_SET_KEY 0
#define MODE_ENCRYPT 1
#define MODE_DECRYPT 2

#ifndef CACHELINE_BYTES
#define CACHELINE_BYTES 128
#endif

// Blowfish Configuration PATTERN.
// This must match with DATA structure in hls_blowfish/kernel.cpp
typedef struct blowfish_job {
    struct snap_addr input_data;
    struct snap_addr output_data; // not needed for MODE_SET_KEY
    uint32_t data_length;
    uint32_t mode;
} blowfish_job_t;

/*
// For later:
typedef struct blowfish_out {
    uint32_t length;
    char * data;
} blowfish_out_t;


int blowfish(char * input_data, unsigned int input_length, char * key, unsigned int key_length, unsigned int mode, blowfish_out_t * output);

*/

#ifdef __cplusplus
}
#endif
#endif	/* __ACTION_BFS_H__ */
