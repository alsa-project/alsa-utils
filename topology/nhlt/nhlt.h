// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#ifndef __NHLT_H
#define __NHLT_H

#include <stdint.h>

/*
 * Nhlt defines and structs are derived from:
 * https://01.org/sites/default/files/595976_intel_sst_nhlt.pdf
 *
 * Acpi description header for example:
 * https://uefi.org/sites/default/files/resources/ACPI_6_3_final_Jan30.pdf
 *
 * Idea is to generate similar blob as you would get from:
 * 'cat /sys/firmware/acpi/tables/NHLT'
 *
 */
#define	NHLT_LINK_TYPE_HDAUDIO 0
#define NHLT_LINK_TYPE_DSP 1
#define NHLT_LINK_TYPE_PDM 2
#define NHLT_LINK_TYPE_SSP 3
#define NHLT_LINK_TYPE_SLIMBUS 4
#define NHLT_LINK_TYPE_SOUNDWIRE 5

#define NHLT_VENDOR_ID_INTEL 0x8086

#define NHLT_DEVICE_ID_INTEL_PDM_DMIC 0xAE20
#define NHLT_DEVICE_ID_INTEL_BT_SIDEBAND 0xAE30
#define NHLT_DEVICE_ID_INTEL_I2S_TDM 0xAE34

#define NHLT_DEVICE_TYPE_SSP_BT_SIDEBAND 0
#define NHLT_DEVICE_TYPE_SSP_FM 1
#define NHLT_DEVICE_TYPE_SSP_MODEM 2
#define NHLT_DEVICE_TYPE_SSP_ANALOG 4

#define NHLT_ENDPOINT_DIRECTION_RENDER 0
#define NHLT_ENDPOINT_DIRECTION_CAPTURE 1
#define NHLT_ENDPOINT_DIRECTION_RENDER_WITH_LOOPBACK 2
#define NHLT_ENDPOINT_DIRECTION_FEEDBACK_FOR_RENDER 3

#define NHLT_DEVICE_CONFIG_TYPE_GENERIC 0
#define NHLT_DEVICE_CONFIG_TYPE_MICARRAY 1
#define NHLT_DEVICE_CONFIG_TYPE_RENDERWITHLOOPBACK 2
#define NHLT_DEVICE_CONFIG_TYPE_RENDERFEEDBACK 3

#define NHLT_MIC_ARRAY_TYPE_LINEAR_2_ELEMENT_SMALL 0xA
#define NHLT_MIC_ARRAY_TYPE_LINEAR_2_ELEMENT_BIG 0xB
#define NHLT_MIC_ARRAY_TYPE_LINEAR_4_ELEMENT_1ST_GEOMETRY 0xC
#define NHLT_MIC_ARRAY_TYPE_PLANAR_4_ELEMENT_L_SHAPED 0xD
#define NHLT_MIC_ARRAY_TYPE_PLANAR_4_ELEMENT_2ND_GEOMETRY 0xE
#define NHLT_MIC_ARRAY_TYPE_VENDOR_DEFINED 0xF

#define NHLT_MIC_ARRAY_NO_EXTENSION 0x0
#define NHLT_MIC_ARRAY_SNR_AND_SENSITIVITY_EXTENSION 0x1

#define NHLT_MIC_TYPE_OMNIDIRECTIONAL 0
#define NHLT_MIC_TYPE_SUBCARDIOID 1
#define NHLT_MIC_TYPE_CARDIOID 2
#define NHLT_MIC_TYPE_SUPERCARDIOID 3
#define NHLT_MIC_TYPE_HYPERCARDIOID 4
#define NHLT_MIC_TYPE_8SHAPED 5
#define NHLT_MIC_TYPE_RESERVED 6
#define NHLT_MIC_TYPE_VENDORDEFINED 7

#define NHLT_MIC_POSITION_TOP 0
#define NHLT_MIC_POSITION_BOTTOM 1
#define NHLT_MIC_POSITION_LEFT 2
#define NHLT_MIC_POSITION_RIGHT 3
#define NHLT_MIC_POSITION_FRONT 4 /*(default) */
#define NHLT_MIC_POSITION_REAR 5

struct specific_config {
	uint32_t capabilities_size; /* does not include size of this field */
	uint8_t capabilities[];
} __attribute__((packed));

struct device_specific_config {
	uint8_t virtual_slot;
	uint8_t config_type;
} __attribute__((packed));

struct ssp_device_specific_config {
	struct specific_config config;
	struct device_specific_config device_config;
} __attribute__((packed));

struct mic_snr_sensitivity_extension {
	uint32_t snr;
	uint32_t sensitivity;
} __attribute__((packed));

struct mic_vendor_config {
	uint8_t type;
	uint8_t panel;
	uint32_t speaker_position_distance;
	uint32_t horizontal_offset;
	uint32_t vertical_offset;
	uint8_t frequency_low_band;
	uint8_t frequency_high_band;
	uint16_t direction_angle;
	uint16_t elevation_angle;
	uint16_t vertical_angle_begin;
	uint16_t vertical_angle_end;
	uint16_t horizontal_angle_begin;
	uint16_t horizontal_angle_end;
} __attribute__((packed));

struct mic_array_device_specific_config {
	struct specific_config config;
	struct device_specific_config device_config;
	uint8_t array_type_ex;
} __attribute__((packed));

struct mic_array_device_specific_vendor_config {
	struct specific_config config;
	struct device_specific_config device_config;
	uint8_t array_type_ex;
	uint8_t number_of_microphones;
	uint8_t mic_vendor_configs[];
} __attribute__((packed));

struct WAVEFORMATEXTENSIBLE {
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	uint16_t cbSize;
	uint16_t wValidBitsPerSample;
	uint32_t dwChannelMask;
	uint32_t SubFormat[4];
} __attribute__((packed));

struct format_config {
	struct WAVEFORMATEXTENSIBLE format;
	struct specific_config vendor_blob;
} __attribute__((packed));

struct formats_config {
	uint8_t formats_count;
	uint8_t f_configs[];
} __attribute__((packed));

struct endpoint_descriptor {
	uint32_t length; /* includes the length of this field also */
	uint8_t link_type;
	uint8_t instance_id;
	uint16_t vendor_id;
	uint16_t device_id;
	uint16_t revision_id;
	uint32_t subsystem_id;
	uint8_t device_type;
	uint8_t direction;
	uint8_t virtualbus_id;
} __attribute__((packed));

struct efi_acpi_description_header {
	uint32_t signature;
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	uint8_t oem_id[6];
	uint64_t oem_table_id;
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} __attribute__((packed));

struct nhlt {
	struct efi_acpi_description_header efi_acpi;
	uint8_t endpoint_count;
	uint8_t endpoints[];
} __attribute__((packed));

#endif /* __NHLT_H */
