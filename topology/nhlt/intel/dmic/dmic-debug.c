// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include <stdio.h>
#include <stdint.h>
#include "dmic-debug.h"

#ifdef NHLT_DEBUG

/* print blob as bytes hex string like: 0x11,0xff,0xff,0xff etc. */
void dmic_print_bytes_as_hex(uint8_t *src, size_t size)
{
	int i, j, lines, remain;

	fprintf(stdout, "printing dmic vendor blob as bytes:\n");

	lines = size / 8;
	remain = size % 8;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < 8; j++) {
			fprintf(stdout, "0x%02x,", *src);
			src++;
		}
		fprintf(stdout, "\n");
	}
	for (i = 0; i < remain; i++) {
		fprintf(stdout, "0x%02x,", *src);
		src++;
	}

	fprintf(stdout, "\n");
}

/* print blob as 32 bit integer hex string like: 0xffffffff,0x00000010 etc. */
void dmic_print_integers_as_hex(uint32_t *src, size_t size)
{
	int i, j, lines, remain;

	fprintf(stdout, "printing dmic vendor blob as integers:\n");

	lines = size / 8;
	remain = size % 8;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < 8; j++) {
			fprintf(stdout, "0x%08x,", *src);
			src++;
		}
		fprintf(stdout, "\n");
	}
	for (i = 0; i < remain; i++) {
		fprintf(stdout, "0x%08x,", *src);
		src++;
	}

	fprintf(stdout, "\n");
}

void dmic_print_internal(struct intel_dmic_params *dmic)
{
	int i, j, line, lines, remain;

	fprintf(stdout, "printing dmic nhlt internal data:\n");

	/* top level struct */
	fprintf(stdout, "gateway attributes: 0x%08x\n", dmic->dmic_blob.gateway_attributes);

	fprintf(stdout, "ts_group: 0x%08x 0x%08x 0x%08x 0x%08x\n", dmic->dmic_blob.ts_group[0],
		dmic->dmic_blob.ts_group[1], dmic->dmic_blob.ts_group[2],
		dmic->dmic_blob.ts_group[3]);

	fprintf(stdout, "clock_on_delay: 0x%08x\n", dmic->dmic_blob.clock_on_delay);

	fprintf(stdout, "channel_ctrl_mask: 0x%08x\n", dmic->dmic_blob.channel_ctrl_mask);

	fprintf(stdout, "chan_ctrl_cfg: 0x%08x 0x%08x\n", dmic->dmic_blob.chan_ctrl_cfg[0],
		dmic->dmic_blob.chan_ctrl_cfg[1]);

	fprintf(stdout, "channel_pdm_mask: 0x%08x\n", dmic->dmic_blob.channel_pdm_mask);

	/* first pdm struct */
	fprintf(stdout, "pdm_ctrl_cfg 0\n");
	fprintf(stdout, "cic_control: 0x%08x\n", dmic->dmic_blob_pdm[0].cic_control);
	fprintf(stdout, "cic_config: 0x%08x\n", dmic->dmic_blob_pdm[0].cic_config);
	fprintf(stdout, "mic_control: 0x%08x\n", dmic->dmic_blob_pdm[0].mic_control);
	fprintf(stdout, "pdmsm: 0x%08x\n", dmic->dmic_blob_pdm[0].pdmsm);
	fprintf(stdout, "reuse_fir_from_pdm: 0x%08x\n", dmic->dmic_blob_pdm[0].reuse_fir_from_pdm);

	/* first pdm struct, first fir */
	fprintf(stdout, "fir_config 0\n");
	fprintf(stdout, "fir_control: 0x%08x\n", dmic->dmic_blob_fir[0][0].fir_control);
	fprintf(stdout, "fir_config: 0x%08x\n", dmic->dmic_blob_fir[0][0].fir_config);
	fprintf(stdout, "dc_offset_left: 0x%08x\n", dmic->dmic_blob_fir[0][0].dc_offset_left);
	fprintf(stdout, "dc_offset_right: 0x%08x\n", dmic->dmic_blob_fir[0][0].dc_offset_right);
	fprintf(stdout, "out_gain_left: 0x%08x\n", dmic->dmic_blob_fir[0][0].out_gain_left);
	fprintf(stdout, "out_gain_right: 0x%08x\n", dmic->dmic_blob_fir[0][0].out_gain_right);

	/* first pdm struct, second fir */
	fprintf(stdout, "fir_config 1\n");
	fprintf(stdout, "fir_control: 0x%08x\n", dmic->dmic_blob_fir[0][1].fir_control);
	fprintf(stdout, "fir_config: 0x%08x\n", dmic->dmic_blob_fir[0][1].fir_config);
	fprintf(stdout, "dc_offset_left: 0x%08x\n", dmic->dmic_blob_fir[0][1].dc_offset_left);
	fprintf(stdout, "dc_offset_right: 0x%08x\n", dmic->dmic_blob_fir[0][1].dc_offset_right);
	fprintf(stdout, "out_gain_left: 0x%08x\n", dmic->dmic_blob_fir[0][1].out_gain_left);
	fprintf(stdout, "out_gain_right: 0x%08x\n", dmic->dmic_blob_fir[0][1].out_gain_right);

	/* first pdm struct, fir coeffs */
	for (j = 0; j < DMIC_HW_CONTROLLERS; j++) {
		fprintf(stdout, "fir_coeffs a length %u:\n", dmic->dmic_fir_array.fir_len[0]);
		lines = dmic->dmic_fir_array.fir_len[0] / 8;
		remain = dmic->dmic_fir_array.fir_len[0] % 8;
		for (i = 0; i < lines; i++) {
			line = i * 8;
			fprintf(stdout, "%d %d %d %d %d %d %d %d %d\n", i,
				dmic->dmic_fir_array.fir_coeffs[j][0][line],
				dmic->dmic_fir_array.fir_coeffs[j][0][line + 1],
				dmic->dmic_fir_array.fir_coeffs[j][0][line + 2],
				dmic->dmic_fir_array.fir_coeffs[j][0][line + 3],
				dmic->dmic_fir_array.fir_coeffs[j][0][line + 4],
				dmic->dmic_fir_array.fir_coeffs[j][0][line + 5],
				dmic->dmic_fir_array.fir_coeffs[j][0][line + 6],
				dmic->dmic_fir_array.fir_coeffs[j][0][line + 7]);
		}
		line += 1;
		for (i = 0; i < remain; i++)
			fprintf(stdout, "%d ", dmic->dmic_fir_array.fir_coeffs[j][0][line + i]);
	}

	/* second pdm struct */
	fprintf(stdout, "pdm_ctrl_cfg 1\n");
	fprintf(stdout, "cic_control: 0x%08x\n", dmic->dmic_blob_pdm[1].cic_control);
	fprintf(stdout, "cic_config: 0x%08x\n", dmic->dmic_blob_pdm[1].cic_config);
	fprintf(stdout, "mic_control: 0x%08x\n", dmic->dmic_blob_pdm[1].mic_control);
	fprintf(stdout, "pdmsm: 0x%08x\n", dmic->dmic_blob_pdm[1].pdmsm);
	fprintf(stdout, "reuse_fir_from_pdm: 0x%08x\n", dmic->dmic_blob_pdm[1].reuse_fir_from_pdm);

	/* second pdm struct, first fir */
	fprintf(stdout, "fir_config 0\n");
	fprintf(stdout, "fir_control: 0x%08x\n", dmic->dmic_blob_fir[1][0].fir_control);
	fprintf(stdout, "fir_config: 0x%08x\n", dmic->dmic_blob_fir[1][0].fir_config);
	fprintf(stdout, "dc_offset_left: 0x%08x\n", dmic->dmic_blob_fir[1][0].dc_offset_left);
	fprintf(stdout, "dc_offset_right: 0x%08x\n", dmic->dmic_blob_fir[1][0].dc_offset_right);
	fprintf(stdout, "out_gain_left: 0x%08x\n", dmic->dmic_blob_fir[1][0].out_gain_left);
	fprintf(stdout, "out_gain_right: 0x%08x\n", dmic->dmic_blob_fir[1][0].out_gain_right);

	/* second pdm struct, second fir */
	fprintf(stdout, "fir_config 1\n");
	fprintf(stdout, "fir_control: 0x%08x\n", dmic->dmic_blob_fir[1][1].fir_control);
	fprintf(stdout, "fir_config: 0x%08x\n", dmic->dmic_blob_fir[1][1].fir_config);
	fprintf(stdout, "dc_offset_left: 0x%08x\n", dmic->dmic_blob_fir[1][1].dc_offset_left);
	fprintf(stdout, "dc_offset_right: 0x%08x\n", dmic->dmic_blob_fir[1][1].dc_offset_right);
	fprintf(stdout, "out_gain_left: 0x%08x\n", dmic->dmic_blob_fir[1][1].out_gain_left);
	fprintf(stdout, "out_gain_right: 0x%08x\n", dmic->dmic_blob_fir[1][1].out_gain_right);

	for (j = 0; j < DMIC_HW_CONTROLLERS; j++) {
		fprintf(stdout, "fir_coeffs b length %u:\n", dmic->dmic_fir_array.fir_len[1]);
		lines = dmic->dmic_fir_array.fir_len[1] / 8;
		remain = dmic->dmic_fir_array.fir_len[1] % 8;
		for (i = 0; i < lines; i++) {
			line = i * 8;
			fprintf(stdout, "%d %d %d %d %d %d %d %d %d\n", i,
				dmic->dmic_fir_array.fir_coeffs[j][1][line],
				dmic->dmic_fir_array.fir_coeffs[j][1][line + 1],
				dmic->dmic_fir_array.fir_coeffs[j][1][line + 2],
				dmic->dmic_fir_array.fir_coeffs[j][1][line + 3],
				dmic->dmic_fir_array.fir_coeffs[j][1][line + 4],
				dmic->dmic_fir_array.fir_coeffs[j][1][line + 5],
				dmic->dmic_fir_array.fir_coeffs[j][1][line + 6],
				dmic->dmic_fir_array.fir_coeffs[j][1][line + 7]);
		}
		line += 1;
		for (i = 0; i < remain; i++)
			fprintf(stdout, "%d ", dmic->dmic_fir_array.fir_coeffs[j][1][line + i]);
	}

	fprintf(stdout, "\n");
}

#else /* NHLT_DEBUG */
void dmic_print_bytes_as_hex(uint8_t *src, size_t size) {}
void dmic_print_integers_as_hex(uint32_t *src, size_t size) {}
void dmic_print_internal(struct intel_dmic_params *dmic) {}
#endif
