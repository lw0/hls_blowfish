/*
 * Minimal Blowfish Demo HLS tryout
 *
 * Experimental, use for real data not reccomended
 *
 * Wikipedia's pages are based on "CC BY-SA 3.0"
 * Creative Commons Attribution-ShareAlike License 3.0
 * https://creativecommons.org/licenses/by-sa/3.0/
 */

/*
 * Copyright 2017 International Business Machines
 * Copyright 2017 Lukas Wenzel, HPI
 * Copyright 2017 Balthasar Martin, HPI
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

#include <snap_tools.h>
#include <libsnap.h>
#include <action_blowfish.h>

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

static uint8_t example_encrypted[64] __attribute__((aligned(64))) = {
    0x00,
};

static uint8_t example_decrypted[64] __attribute__((aligned(64))) = {
    0x00,
};

static uint8_t example_key[] __attribute__((aligned(64))) = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
};


static void snap_prepare_blowfish(struct snap_job *job,
        uint32_t mode_in,
        uint32_t data_length_in,
        blowfish_job_t *bjob_in,
        blowfish_job_t *bjob_out,
        const void *addr_in,
        uint16_t type_in,
        void *addr_out,
        uint16_t type_out)
{
    snap_addr_set(&bjob_in->input_data, addr_in, data_length_in,
		  type_in, SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_SRC);

    snap_addr_set(&bjob_in->output_data, addr_out, data_length_in,
		  type_out, SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_DST |
		  SNAP_ADDRFLAG_END );

    bjob_in->mode = mode_in;
    bjob_in->data_length = data_length_in;

    // Here sets the 108byte MMIO settings input.
    // We have input parameters.
    snap_job_set(job, bjob_in, sizeof(*bjob_in),
		 bjob_out, sizeof(*bjob_out));
}

static int blowfish_set_key(struct snap_action *action, unsigned long timeout,
                const uint8_t *key, unsigned int length)
{
    struct snap_job job;
    blowfish_job_t bjob_in;

    // input validation
    if (length < 4 || length > 56 || length % 4 != 0) {
        printf("err: key has to be multiple of 4 bytes with a length "
               "between 4 and 56 bytes!\n");
        return -EINVAL;
        }
    
    snap_prepare_blowfish(&job, MODE_SET_KEY, length, &bjob_in, NULL,
                  key, SNAP_ADDRTYPE_HOST_DRAM,
                  NULL, SNAP_ADDRTYPE_UNUSED);

    snap_action_sync_execute_job(action, &job, timeout);

    return 0;
}

static int blowfish_cipher(struct snap_action *action,
               int mode, unsigned long timeout,
               const uint8_t *ibuf,
               unsigned int in_len,
               uint8_t *obuf)
{
    struct snap_job job;
    blowfish_job_t bjob_in;

    if (in_len % 8 != 0 || in_len <= 0) {
        printf("err: data to en- or decrypt has to be multiple of "
               "8 bytes and at least 8 bytes long!\n");
        return -EINVAL;
    }

    snap_prepare_blowfish(&job, mode, in_len, &bjob_in, NULL,
                  (void *)ibuf, SNAP_ADDRTYPE_HOST_DRAM,
                  (void *)obuf, SNAP_ADDRTYPE_HOST_DRAM);

    snap_action_sync_execute_job(action, &job, timeout);

    return 0;
}


static int blowfish_test(struct snap_action *action, unsigned long timeout)
{

    /* Set a key */
    blowfish_set_key(action, timeout, example_key, sizeof(example_key));

    fprintf(stdout, "Key:\n");
    __hexdump(stdout, example_key, sizeof(example_key));

    fprintf(stdout, "Original plaintext:\n");
    __hexdump(stdout, example_plaintext, sizeof(example_plaintext));

    /* Encrypt data */
    blowfish_cipher(action, MODE_ENCRYPT, timeout,
                 example_plaintext, sizeof(example_plaintext),
                 example_encrypted);

    fprintf(stdout, "Encrypted:\n");
    __hexdump(stdout, example_encrypted, sizeof(example_encrypted));

    /* Decrypt data */
    blowfish_cipher(action, MODE_DECRYPT, timeout,
                 example_encrypted, sizeof(example_encrypted),
                 example_decrypted);
    
    fprintf(stdout, "Decrypted:\n");
    __hexdump(stdout, example_decrypted, sizeof(example_decrypted));

    /* Verification */
    if (memcmp(example_plaintext, example_decrypted,
           sizeof(example_plaintext)) != 0) {
        fprintf(stderr, "ERROR: Data does not match!!\n");
        return -4;
    }
    return 0;
}

int main()
{
    int card_no = 0;
    unsigned long timeout = 10000;
    struct snap_card *card = NULL;
    struct snap_action *action = NULL;
    snap_action_flag_t action_irq = (SNAP_ACTION_DONE_IRQ | SNAP_ATTACH_IRQ);
    
    // Hardcoded card number for simplicity
    card = snap_card_alloc_dev("/dev/cxl/afu0.0s", SNAP_VENDOR_ID_IBM, SNAP_DEVICE_ID_SNAP);
    
    if (card == NULL) {
        fprintf(stderr, "err: failed to open card %u: %s\n",
            card_no, strerror(errno));
            return 1;
    }

    action = snap_attach_action(card, BLOWFISH_ACTION_TYPE, action_irq, 60);
    blowfish_test(action, timeout);
    return 0;
}
