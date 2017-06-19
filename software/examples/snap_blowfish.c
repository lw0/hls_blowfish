/*
 * Simple Breadth-first-search in C
 *
 * Use Adjacency list to describe a graph:
 *        https://en.wikipedia.org/wiki/Adjacency_list
 *
 * Wikipedia's pages are based on "CC BY-SA 3.0"
 * Creative Commons Attribution-ShareAlike License 3.0
 * https://creativecommons.org/licenses/by-sa/3.0/
 */

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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <snap_tools.h>
#include <libsnap.h>
#include <action_blowfish.h>
#include <snap_hls_if.h>


/*
 * BFS: breadth first search
 *    Simple demo to traverse a graph stored in adjcent table.
 *
 *    A directed graph with vertexs (or called node) and edges (or called arc)
 *    The adjacent table format is:
 *    vex_list[0]   -> {edge, vex_index} -> {edge, vex_index} -> ... -> NULL
 *    vex_list[1]   -> {edge, vex_index} -> {edge, vex_index} -> ... -> NULL
 *    ...
 *    vex_list[N-1] -> {edge, vex_index} -> {edge, vex_index} -> ... -> NULL
 *
 * Function:
 *    Starting from each vertex node (called 'root'),
 *      and search all of the vertexes that it can reach.
 *      Visited nodes are recorded in obuf.
 *
 * Implementation:
 *    We ask FPGA to visit the host memory to traverse this data structure.
 *    1. We need to set a BFS_ACTION_TYPE, this is the ACTION ID.
 *    2. We need to fill in 108 bytes configuration space.
 *    Host will send this field to FPGA via MMIO-32.
 *          This field is completely user defined. see 'bfs_job_t'
 *    3. Call snap APIs
 *
 * Notes:
 *    When 'timeout' is reached, PSLSE will send ha_jcom=LLCMD (0x45) and uncompleted transactions will be killed.
 *
 */

int verbose_flag = 0;

/*---------------------------------------------------
 *       Sample Data
 *---------------------------------------------------*/

static unsigned char example_plaintext[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
static unsigned char example_key[] = { 0x00, 0x11, 0x22 , 0x33, 0x44, 0x55, 0x66, 0x77};

static void print_bytes(unsigned char* bytes, unsigned int length)
{
    unsigned int i =0;
    while (i < length)
    {
         fprintf(stdout, "%02X",(unsigned)bytes[i]);
         i++;
    }
    fprintf(stdout, "\n");
}

/*---------------------------------------------------
 *       Hook 108B Configuration
 *---------------------------------------------------*/

static void snap_prepare_blowfish(struct snap_job *job,
        uint32_t mode_in,
        uint32_t data_length_in,
        blowfish_job_t *bjob_in,
        blowfish_job_t *bjob_out,
        void *addr_in,
        uint16_t type_in,

        void *addr_out,
        uint16_t type_out)
{

    fprintf(stdout, "----------------  Config Space ----------- \n");
    fprintf(stdout, "mode = %d\n", mode_in);
    fprintf(stdout, "input_address = %p -> ",addr_in);
    print_bytes((unsigned char*) addr_in, data_length_in);
    fprintf(stdout, "output_address = %p -> ", addr_out);
    print_bytes((unsigned char*) addr_out, data_length_in);
    fprintf(stdout, "data_length = %d\n", data_length_in);
    fprintf(stdout, "------------------------------------------ \n");

    snap_addr_set(&bjob_in->input_data, addr_in, 0,
            type_in, SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_SRC);

    snap_addr_set(&bjob_in->output_data, addr_out, 0,
            type_out, SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_DST | SNAP_ADDRFLAG_END );

    bjob_in->mode = mode_in;
    bjob_in->data_length = data_length_in;

    // Here sets the 108byte MMIO settings input.
    // We have input parameters.
    snap_job_set(job, bjob_in, sizeof(*bjob_in),
            bjob_out, sizeof(*bjob_out));
}

/*---------------------------------------------------
 *       MAIN
 *---------------------------------------------------*/

static void blowfish_operation(unsigned char* data, unsigned int length, unsigned int mode) {
    // input validation
    if (mode == MODE_SET_KEY) {
        if (length < 4 || length > 56 || length % 4 != 0) {
            printf("err: key has to be multiple of 4 bytes with a length between 4 and 56 bytes!\n");
            goto input_error;
        }
    }
    if (mode == MODE_ENCRYPT || mode == MODE_DECRYPT) {
        if (length % 8 != 0 || length <= 0) {
            printf("err: data to en- or decrypt has to be multiple of 8 bytes and at least 8 bytes long!\n");
            goto input_error;
        }
    }

    int rc = 0;
    int card_no = 0;
    struct snap_card *card = NULL;
    struct snap_action *action = NULL;
    char device[128];
    struct snap_job job;
    struct timeval etime, stime;
    uint32_t page_size = sysconf(_SC_PAGESIZE);

    unsigned long timeout = 10000;
    snap_action_flag_t action_irq = 0;

    //Action specfic
    blowfish_job_t bjob_in;
    blowfish_job_t bjob_out;

    //Input buffer
    uint8_t type_in = SNAP_ADDRTYPE_HOST_DRAM;
    unsigned char * ibuf = 0x0ull;

    //Output buffer
    uint8_t type_out = SNAP_ADDRTYPE_HOST_DRAM;
    unsigned char * obuf = 0x0ull;

    // allocate multiple of cacheline size (128 byte)
    unsigned int in_size = (length/128 + 1) * 128;
    unsigned int out_size = 0;
    if (mode == MODE_ENCRYPT || mode == MODE_DECRYPT) {
        out_size = in_size;
    }

    ibuf = memalign(page_size, in_size);
    memcpy(ibuf, data, length); // TODO check if neccessary, or if data[] could directly be used
    obuf = memalign(page_size, out_size);

    //////////////////////////////////////////////////////////////////////

    fprintf(stdout, "snap_kernel_attach start...\n");

    snprintf(device, sizeof(device)-1, "/dev/cxl/afu%d.0s", card_no);
    card = snap_card_alloc_dev(device, SNAP_VENDOR_ID_IBM,
            SNAP_DEVICE_ID_SNAP);
    if (card == NULL) {
        fprintf(stderr, "err: failed to open card %u: %s\n",
                card_no, strerror(errno));
        goto out_error;
    }

    
    action = snap_attach_action(card, BLOWFISH_ACTION_TYPE, action_irq, 60);
    if (action == NULL) {
        fprintf(stderr, "err: failed to attach action %u: %s\n",
                card_no, strerror(errno));
        goto out_error1;
    }

    snap_prepare_blowfish(&job, mode, length,
            &bjob_in, &bjob_out,
            (void *)ibuf, type_in,
            (void *)obuf, type_out);

    fprintf(stdout, "INFO: Timer starts...\n");
    gettimeofday(&stime, NULL);
    rc = snap_action_sync_execute_job(action, &job, timeout);
    gettimeofday(&etime, NULL);
    if (rc != 0) {
        fprintf(stderr, "err: job execution %d: %s!\n", rc,
                strerror(errno));
        goto out_error2;
    }

    fprintf(stdout, "RETC=%x\n", job.retc);
    fprintf(stdout, "INFO: Blowfish took %lld usec\n",
            (long long)timediff_usec(&etime, &stime));
    fprintf(stdout, "------------------------------------------ \n");

    switch (mode) {
        case MODE_SET_KEY: 
            fprintf(stdout, "Key set to ");
            print_bytes(data, length);
            break;
        case MODE_ENCRYPT:
            fprintf(stdout, "Plaintext is at %p -> ", (void * )bjob_in.input_data.addr);
            print_bytes((unsigned char*) bjob_in.input_data.addr, bjob_in.data_length);
            fprintf(stdout, "Cipher is at %p -> ", (void *)bjob_in.output_data.addr);
            print_bytes((unsigned char*) bjob_in.output_data.addr, bjob_in.data_length);
            break;
        case MODE_DECRYPT:
            fprintf(stdout, "Cipher is at %p -> ", (void *)bjob_in.input_data.addr);
            print_bytes((unsigned char*) bjob_in.input_data.addr, bjob_in.data_length);
            fprintf(stdout, "Plaintext is at %p -> ", (void *)bjob_in.output_data.addr);
            print_bytes((unsigned char*) bjob_in.output_data.addr, bjob_in.data_length);
            break;
    }
    snap_detach_action(action);
    snap_card_free(card);
    free(ibuf);
    free(obuf);
    
    return;

out_error2:
    snap_detach_action(action);
out_error1:
    snap_card_free(card);
out_error:
    free(ibuf);
    free(obuf);
input_error:
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    //General variables for snap call
    argc = argc;
    argv = argv;
    //print_bytes(example_plaintext, sizeof(example_plaintext));

    blowfish_operation(example_key, sizeof(example_key), MODE_SET_KEY);
    blowfish_operation(example_plaintext, sizeof(example_plaintext), MODE_ENCRYPT);
    exit(0);
}
