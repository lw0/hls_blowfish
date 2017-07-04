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
 * Copyright 2017 International Business Machines
 * Copyright 2017 Lukas Wenzel, HPI
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

static const char *version = GIT_VERSION;
int verbose_flag = 0;

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

/*---------------------------------------------------
 *       Sample Data
 *---------------------------------------------------*/

/*
 * FIXME If you like to use those pointers directly, use an gcc's alignment
 *       attributes and ensure that it is 64 byte aligned.
 *
 * E.g. like this:
 *   __attribute__((align(64)));
 */
static const uint8_t example_plaintext[] __attribute__((aligned(64))) = {
	0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, /*  8 bytes */
	0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, /* 16 bytes */
	0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, /* 24 bytes */
	0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, /* 32 bytes */
	0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, /* 40 bytes */
	0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, /* 48 bytes */
	0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, /* 56 bytes */
	0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, /* 64 bytes */
};

static unsigned char example_key[] __attribute__((aligned(64))) = {
	0x00, 0x11, 0x22 , 0x33, 0x44, 0x55, 0x66, 0x77
};

static void print_bytes(const uint8_t *bytes, unsigned int length)
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
    static const char *mode_str[] = 
	    { "MODE_SET_KEY", "MODE_ENCRYPT", "MODE_DECRYPT" };

    fprintf(stderr, "----------------  Config Space ----------- \n");
    fprintf(stderr, "mode = %d %s\n", mode_in, mode_str[mode_in % 3]);
    fprintf(stderr, "input_address = %p -> ",addr_in);
    print_bytes((unsigned char*) addr_in, 128);
    fprintf(stderr, "output_address = %p -> ", addr_out);
    print_bytes((unsigned char*) addr_out, 128);
    fprintf(stderr, "data_length = %d\n", data_length_in);
    fprintf(stderr, "------------------------------------------ \n");

    snap_addr_set(&bjob_in->input_data, addr_in, data_length_in,
            type_in, SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_SRC);

    snap_addr_set(&bjob_in->output_data, addr_out, data_length_in,
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

static void blowfish_operation(int card_no,
			       unsigned long timeout,
			       const uint8_t *data,
			       unsigned int length,
			       unsigned int mode)
{
    // input validation
    if (mode == MODE_SET_KEY) {
        if (length < 4 || length > 56 || length % 4 != 0) {
            printf("err: key has to be multiple of 4 bytes with a length "
		   "between 4 and 56 bytes!\n");
            goto input_error;
        }
    }
    if (mode == MODE_ENCRYPT || mode == MODE_DECRYPT) {
        if (length % 8 != 0 || length <= 0) {
            printf("err: data to en- or decrypt has to be multiple of "
		   "8 bytes and at least 8 bytes long!\n");
            goto input_error;
        }
    }

    int rc = 0;
    struct snap_card *card = NULL;
    struct snap_action *action = NULL;
    char device[128];
    struct snap_job job;
    struct timeval etime, stime;
    uint32_t page_size = sysconf(_SC_PAGESIZE);
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

    /* FIXME Always check memalign return code! */
    ibuf = memalign(page_size, in_size);
    memcpy(ibuf, data, length);

    // TODO check if neccessary, or if data[] could directly be used
    // data can only be used if it is aligned properly.

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
            fprintf(stdout, "Plaintext is at %p -> ", (void * )bjob_out.input_data.addr);
            print_bytes((unsigned char*) bjob_out.input_data.addr, 128);
            fprintf(stdout, "Cipher is at %p -> ", (void *)bjob_out.output_data.addr);
            fprintf(stdout, "Cipher is at %p -> ", (void *)obuf);
            print_bytes((unsigned char*) obuf, 128);
            break;
        case MODE_DECRYPT:
            fprintf(stdout, "Cipher is at %p -> ", (void *)bjob_out.input_data.addr);
            print_bytes((unsigned char*) bjob_out.input_data.addr, 128);
            fprintf(stdout, "Plaintext is at %p -> ", (void *)bjob_out.output_data.addr);
            print_bytes((unsigned char*) bjob_out.output_data.addr, 128);
            break;
    }

    fprintf(stderr, "Input Buffer:\n");
    __hexdump(stderr, ibuf, in_size);

    fprintf(stderr, "Output Buffer:\n");
    __hexdump(stderr, obuf, out_size);

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

/**
 * @brief       Print valid command line options
 * @param prog  Current program name
 */
static void usage(const char *prog)
{
        printf("Usage: %s [-h] [-v, --verbose] [-V, --version]\n"
               "  -C, --card <cardno> can be (0...3)\n"
               "  -i, --input <file.bin>    input file.\n"
               "  -o, --output <file.bin>   output file.\n"
	       "  -c, --crypt\n"
	       "  -d, --decrypt\n"
               "  -t, --timeout             Timeout in sec to wait for donen"
               "\n"
               "Example:\n"
               "  snap_blowfish ...\n"
               "\n",
               prog);
}

int main(int argc, char *argv[])
{
        int card_no = 0;
	int decrypt = 0;
	const char *input = NULL;
        const char *output = NULL;
        unsigned long timeout = 10000;
	int ch;

        while (1) {
                int option_index = 0;
                static struct option long_options[] = {
                        { "card",        required_argument, NULL, 'C' },
                        { "input",       required_argument, NULL, 'i' },
                        { "output",      required_argument, NULL, 'o' },
                        { "decrypt",     no_argument,       NULL, 'd' },
                        { "timeout",     required_argument, NULL, 't' },
                        { "version",     no_argument,       NULL, 'V' },
                        { "verbose",     no_argument,       NULL, 'v' },
                        { "help",        no_argument,       NULL, 'h' },
                        { 0,             no_argument,       NULL, 0   },
                };

                ch = getopt_long(argc, argv, "C:i:o:dt:Vvh",
                                 long_options, &option_index);
                if (ch == -1)
                        break;

                switch (ch) {
                case 'C':
                        card_no = strtol(optarg, (char **)NULL, 0);
                        break;
                case 'i':
                        input = optarg;
                        break;
                case 'o':
                        output = optarg;
                        break;
		case 'd':
			decrypt = 1;
			break;
                case 't':
                        timeout = strtol(optarg, (char **)NULL, 0);
                        break;
                case 'V':
                        printf("%s\n", version);
                        exit(EXIT_SUCCESS);
                case 'v':
                        verbose_flag++;
                        break;
                case 'h':
                        usage(argv[0]);
                        exit(EXIT_SUCCESS);
                        break;
                default:
                        usage(argv[0]);
                        exit(EXIT_FAILURE);
                }
	}

	fprintf(stderr,	"Blowfish Cypher\n"
		"  operation: %s\n"
		"  input: %s\n"
		"  output: %s\n",
		decrypt ? "decrypt" : "encrypt", input, output);

	if (verbose_flag)
		print_bytes(example_plaintext, sizeof(example_plaintext));

	blowfish_operation(card_no, timeout, example_key, sizeof(example_key),
			   MODE_SET_KEY);
	blowfish_operation(card_no, timeout, example_plaintext, sizeof(example_plaintext),
			   MODE_ENCRYPT);

	exit(EXIT_SUCCESS);
}
