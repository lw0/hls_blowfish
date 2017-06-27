/*
 * Copyright 2017 International Business Machines
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

/*
 * Example to use the FPGA to find patterns in a byte-stream.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libsnap.h>
#include <linux/types.h>	/* __be64 */
#include <asm/byteorder.h>

#include <snap_internal.h>
#include <snap_tools.h>
#include <action_blowfish.h>

static int action_main(struct snap_sim_action *action,
		       void *job, unsigned int job_len)
{
	struct blowfish_job *js = (struct blowfish_job *)job;

	__hexdump(stderr, js, sizeof(*js));

	if (sizeof(*js) != job_len)
		goto err_out;

	/*
	 * FIXME Functional code goes in here. Unfortunately no C++ yet and
	 * no special HLS constructs.
	 */

	action->job.retc = SNAP_RETC_SUCCESS;
	return 0;
 err_out:
	action->job.retc = SNAP_RETC_FAILURE;
	return 0;
}

static struct snap_sim_action action = {
	.vendor_id = SNAP_VENDOR_ID_ANY,
	.device_id = SNAP_DEVICE_ID_ANY,
	.action_type = BLOWFISH_ACTION_TYPE,

	.job = { .retc = SNAP_RETC_FAILURE, },
	.state = ACTION_IDLE,
	.main = action_main,
	.priv_data = NULL,
	.mmio_write32 = NULL,
	.mmio_read32 = NULL,

	.next = NULL,
};

static void _init(void) __attribute__((constructor));

static void _init(void)
{
	snap_action_register(&action);
}
