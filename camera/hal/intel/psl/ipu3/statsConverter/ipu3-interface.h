/*
 * Copyright (C) 2017 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PSL_IPU3_IPU3_INTERFACE_H_
#define PSL_IPU3_IPU3_INTERFACE_H_

#include <stdint.h>
#include <stdbool.h>
#include <linux/intel-ipu3.h>

#define __aligned(x)            __attribute__((aligned(x)))
#define s64 int64_t
#define u64 uint64_t
#define u32 uint32_t
#define s32 int32_t
#define u16 uint16_t
#define s16 int16_t
#define u8 uint8_t
#define s8 int8_t

#define IMGU_ABI_ISP_WORD_BYTES				32
#define IMGU_ABI_MAX_STRIPES				2

/******************* imgu_abi_stats_3a *******************/

#define IMGU_ABI_MAX_BUBBLE_SIZE			10

#define IMGU_ABI_AE_COLORS				4
#define IMGU_ABI_AE_BINS				256

#define IMGU_ABI_AWB_MD_ITEM_SIZE			8
#define IMGU_ABI_AWB_MAX_SETS				60
#define IMGU_ABI_AWB_SET_SIZE				0x500
#define IMGU_ABI_AWB_SPARE_FOR_BUBBLES \
		(IMGU_ABI_MAX_BUBBLE_SIZE * IMGU_ABI_MAX_STRIPES * \
		 IMGU_ABI_AWB_MD_ITEM_SIZE)
#define IMGU_ABI_AWB_MAX_BUFFER_SIZE \
		(IMGU_ABI_AWB_MAX_SETS * \
		 (IMGU_ABI_AWB_SET_SIZE + IMGU_ABI_AWB_SPARE_FOR_BUBBLES))

#define IMGU_ABI_AF_MAX_SETS				24
#define IMGU_ABI_AF_MD_ITEM_SIZE			4
#define IMGU_ABI_AF_SPARE_FOR_BUBBLES \
		(IMGU_ABI_MAX_BUBBLE_SIZE * IMGU_ABI_MAX_STRIPES * \
		 IMGU_ABI_AF_MD_ITEM_SIZE)
#define IMGU_ABI_AF_Y_TABLE_SET_SIZE			0x80
#define IMGU_ABI_AF_Y_TABLE_MAX_SIZE \
	(IMGU_ABI_AF_MAX_SETS * \
	 (IMGU_ABI_AF_Y_TABLE_SET_SIZE + IMGU_ABI_AF_SPARE_FOR_BUBBLES) * \
	 IMGU_ABI_MAX_STRIPES)

#define IMGU_ABI_AWB_FR_MAX_SETS			24
#define IMGU_ABI_AWB_FR_MD_ITEM_SIZE			8
#define IMGU_ABI_AWB_FR_BAYER_TBL_SIZE			0x100
#define IMGU_ABI_AWB_FR_SPARE_FOR_BUBBLES \
 		(IMGU_ABI_MAX_BUBBLE_SIZE * IMGU_ABI_MAX_STRIPES * \
		IMGU_ABI_AWB_FR_MD_ITEM_SIZE)
#define IMGU_ABI_AWB_FR_BAYER_TABLE_MAX_SIZE \
	(IMGU_ABI_AWB_FR_MAX_SETS * \
	(IMGU_ABI_AWB_FR_BAYER_TBL_SIZE + IMGU_ABI_AWB_FR_SPARE_FOR_BUBBLES) * \
	IMGU_ABI_MAX_STRIPES)

struct imgu_abi_grid_config {
    u8 width;				/* 6 or 7 (rgbs_grd_cfg) bits */
    u8 height;
    u16 block_width_log2:3;
    u16 block_height_log2:3;
    u16 height_per_slice:8;				/* default value 1 */
    u16 x_start;					/* 12 bits */
    u16 y_start;
#define IMGU_ABI_GRID_START_MASK			((1 << 12) - 1)
#define IMGU_ABI_GRID_Y_START_EN			(1 << 15)
    u16 x_end;					/* 12 bits */
    u16 y_end;
};

struct imgu_abi_awb_meta_data {
    u8 meta_data_buffer[IMGU_ABI_AWB_MAX_BUFFER_SIZE];
};

struct imgu_abi_awb_raw_buffer {
    struct imgu_abi_awb_meta_data meta_data;
};

struct IMGU_ABI_PAD imgu_abi_awb_config_s {
    u16 rgbs_thr_gr;
    u16 rgbs_thr_r;
    u16 rgbs_thr_gb;
    u16 rgbs_thr_b;
    /* controls generation of meta_data (like FF enable/disable) */
#define IMGU_ABI_AWB_RGBS_THR_B_EN		(1 << 14)
#define IMGU_ABI_AWB_RGBS_THR_B_INCL_SAT	(1 << 15)

    struct imgu_abi_grid_config rgbs_grd_cfg;
};

struct imgu_abi_ae_raw_buffer {
    u32 vals[IMGU_ABI_AE_BINS * IMGU_ABI_AE_COLORS];
};

struct imgu_abi_ae_raw_buffer_aligned {
    struct imgu_abi_ae_raw_buffer buff IMGU_ABI_PAD;
};

struct IMGU_ABI_PAD imgu_abi_ae_grid_config {
    u8 width;
    u8 height;
    u8 block_width_log2:4;
    u8 block_height_log2:4;
    u8 __reserved0:5;
    u8 ae_en:1;
    u8 rst_hist_array:1;
    u8 done_rst_hist_array:1;
    u16 x_start;			/* 12 bits */
    u16 y_start;
    u16 x_end;
    u16 y_end;
};

struct imgu_abi_af_filter_config {
    struct {
        u8 a1;
        u8 a2;
        u8 a3;
        u8 a4;
    } y1_coeff_0;
    struct {
        u8 a5;
        u8 a6;
        u8 a7;
        u8 a8;
    } y1_coeff_1;
    struct {
        u8 a9;
        u8 a10;
        u8 a11;
        u8 a12;
    } y1_coeff_2;

    u32 y1_sign_vec;

    struct {
        u8 a1;
        u8 a2;
        u8 a3;
        u8 a4;
    } y2_coeff_0;
    struct {
        u8 a5;
        u8 a6;
        u8 a7;
        u8 a8;
    } y2_coeff_1;
    struct {
        u8 a9;
        u8 a10;
        u8 a11;
        u8 a12;
    } y2_coeff_2;

    u32 y2_sign_vec;

    struct {
        u8 y_gen_rate_gr;			/* 6 bits */
        u8 y_gen_rate_r;
        u8 y_gen_rate_b;
        u8 y_gen_rate_gb;
    } y_calc;

    struct {
        u32 __reserved0:8;
        u32 y1_nf:4;
        u32 __reserved1:4;
        u32 y2_nf:4;
        u32 __reserved2:12;
    } nf;
};

struct imgu_abi_af_meta_data {
    u8 y_table[IMGU_ABI_AF_Y_TABLE_MAX_SIZE] IMGU_ABI_PAD;
};

struct imgu_abi_af_raw_buffer {
    struct imgu_abi_af_meta_data meta_data IMGU_ABI_PAD;
};

struct imgu_abi_af_frame_size {
    u16 width;
    u16 height;
};

struct imgu_abi_af_config_s {
    struct imgu_abi_af_filter_config filter_config IMGU_ABI_PAD;
    struct imgu_abi_af_frame_size frame_size;
    struct imgu_abi_grid_config grid_cfg IMGU_ABI_PAD;
};

struct imgu_abi_awb_fr_meta_data {
    u8 bayer_table[IMGU_ABI_AWB_FR_BAYER_TABLE_MAX_SIZE] IMGU_ABI_PAD;
};

struct imgu_abi_awb_fr_raw_buffer {
    struct imgu_abi_awb_fr_meta_data meta_data;
};

struct IMGU_ABI_PAD imgu_abi_awb_fr_config_s {
    struct imgu_abi_grid_config grid_cfg;
    u8 bayer_coeff[6];
    u16 __reserved1;
    u32 bayer_sign;			/* 11 bits */
    u8 bayer_nf;			/* 4 bits */
    u8 __reserved2[3];
};

struct imgu_abi_4a_config {
    struct imgu_abi_awb_config_s awb_config IMGU_ABI_PAD;
    struct imgu_abi_ae_grid_config ae_grd_config IMGU_ABI_PAD;
    struct imgu_abi_af_config_s af_config IMGU_ABI_PAD;
    struct imgu_abi_awb_fr_config_s awb_fr_config IMGU_ABI_PAD;
};

struct imgu_abi_bubble_info {
    u32 num_of_stripes IMGU_ABI_PAD;
    u32 num_sets IMGU_ABI_PAD;
    u32 size_of_set IMGU_ABI_PAD;
    u32 bubble_size IMGU_ABI_PAD;
};

struct stats_3a_imgu_abi_bubble_info_per_stripe {
    struct imgu_abi_bubble_info
        awb_imgu_abi_bubble_info[IMGU_ABI_MAX_STRIPES];
    struct imgu_abi_bubble_info
        af_imgu_abi_bubble_info[IMGU_ABI_MAX_STRIPES];
    struct imgu_abi_bubble_info
        awb_fr_imgu_abi_bubble_info[IMGU_ABI_MAX_STRIPES];
};

struct imgu_abi_ff_status {
    u32 awb_en IMGU_ABI_PAD;
    u32 ae_en IMGU_ABI_PAD;
    u32 af_en IMGU_ABI_PAD;
    u32 awb_fr_en IMGU_ABI_PAD;
};

struct imgu_abi_stats_3a {
    struct imgu_abi_awb_raw_buffer awb_raw_buffer IMGU_ABI_PAD;
    struct imgu_abi_ae_raw_buffer_aligned
        ae_raw_buffer[IMGU_ABI_MAX_STRIPES] IMGU_ABI_PAD;
    struct imgu_abi_af_raw_buffer af_raw_buffer IMGU_ABI_PAD;
    struct imgu_abi_awb_fr_raw_buffer awb_fr_raw_buffer IMGU_ABI_PAD;
    struct imgu_abi_4a_config stats_4a_config IMGU_ABI_PAD;
    u32 ae_join_buffers IMGU_ABI_PAD;
    struct stats_3a_imgu_abi_bubble_info_per_stripe
        stats_3a_bubble_per_stripe IMGU_ABI_PAD;
    struct imgu_abi_ff_status stats_3a_status IMGU_ABI_PAD;
};

/******************* imgu_abi_stats_dvs *******************/

#define IMGU_ABI_DVS_STAT_LEVELS			3
#define IMGU_ABI_DVS_STAT_L0_MV_VEC_PER_SET		12
#define IMGU_ABI_DVS_STAT_L1_MV_VEC_PER_SET		11
#define IMGU_ABI_DVS_STAT_L2_MV_VEC_PER_SET		9
#define IMGU_ABI_DVS_STAT_STRIPE_ALIGN_GAP		IMGU_ABI_MAX_STRIPES
#define IMGU_ABI_DVS_STAT_MAX_VERTICAL_FEATURES		16

struct imgu_abi_dvs_stat_mv {
    u16 vec_fe_x_pos;		/* 12 bits */
    u16 vec_fe_y_pos;
    u16 vec_fm_x_pos;		/* 12 bits */
    u16 vec_fm_y_pos;
    u32 harris_grade;		/* 28 bits */
    u16 match_grade;		/* 15 bits */
    u16 level;			/* 3 bits */
};

struct imgu_abi_dvs_stat_mv_single_set_l0 {
    struct imgu_abi_dvs_stat_mv
        mv_entry[IMGU_ABI_DVS_STAT_L0_MV_VEC_PER_SET +
                 IMGU_ABI_DVS_STAT_STRIPE_ALIGN_GAP] IMGU_ABI_PAD;
};

struct imgu_abi_dvs_stat_mv_single_set_l1 {
    struct imgu_abi_dvs_stat_mv
        mv_entry[IMGU_ABI_DVS_STAT_L1_MV_VEC_PER_SET +
                 IMGU_ABI_DVS_STAT_STRIPE_ALIGN_GAP] IMGU_ABI_PAD;
};

struct imgu_abi_dvs_stat_mv_single_set_l2 {
    struct imgu_abi_dvs_stat_mv
        mv_entry[IMGU_ABI_DVS_STAT_L2_MV_VEC_PER_SET +
                 IMGU_ABI_DVS_STAT_STRIPE_ALIGN_GAP] IMGU_ABI_PAD;
};

struct imgu_abi_dvs_stat_motion_vec {
    struct imgu_abi_dvs_stat_mv_single_set_l0
        dvs_mv_output_l0[IMGU_ABI_DVS_STAT_MAX_VERTICAL_FEATURES]
        IMGU_ABI_PAD;
    struct imgu_abi_dvs_stat_mv_single_set_l1
        dvs_mv_output_l1[IMGU_ABI_DVS_STAT_MAX_VERTICAL_FEATURES]
        IMGU_ABI_PAD;
    struct imgu_abi_dvs_stat_mv_single_set_l2
        dvs_mv_output_l2[IMGU_ABI_DVS_STAT_MAX_VERTICAL_FEATURES]
        IMGU_ABI_PAD;
};

struct imgu_abi_dvs_stat_stripe_data {
    u8 grid_width[IMGU_ABI_MAX_STRIPES][IMGU_ABI_DVS_STAT_LEVELS];
    u16 stripe_offset;
};

struct imgu_abi_dvs_stat_gbl_config {
    u8 kappa;					/* 4 bits */
    u8 match_shift:4;
    u8 ybin_mode:1;
    u16 __reserved1;
};

struct imgu_abi_dvs_stat_grd_config {
    u8 grid_width;					/* 5 bits */
    u8 grid_height;
    u8 block_width;					/* 8 bits */
    u8 block_height;
    u16 x_start;					/* 12 bits */
    u16 y_start;
    u16 enable;
    u16 x_end;					/* 12 bits */
    u16 y_end;
};

struct imgu_abi_dvs_stat_fe_roi_cfg {
    u8 x_start;
    u8 y_start;
    u8 x_end;
    u8 y_end;
};

struct imgu_abi_dvs_stat_cfg {
    struct imgu_abi_dvs_stat_gbl_config gbl_cfg;
    struct imgu_abi_dvs_stat_grd_config
        grd_config[IMGU_ABI_DVS_STAT_LEVELS];
    struct imgu_abi_dvs_stat_fe_roi_cfg
        fe_roi_cfg[IMGU_ABI_DVS_STAT_LEVELS];
    u8 __reserved[IMGU_ABI_ISP_WORD_BYTES -
                  (sizeof(struct imgu_abi_dvs_stat_gbl_config) +
                   (sizeof(struct imgu_abi_dvs_stat_grd_config) +
                    sizeof(struct imgu_abi_dvs_stat_fe_roi_cfg)) *
                   IMGU_ABI_DVS_STAT_LEVELS) % IMGU_ABI_ISP_WORD_BYTES];
};

struct imgu_abi_stats_dvs {
    struct imgu_abi_dvs_stat_motion_vec motion_vec IMGU_ABI_PAD;
    struct imgu_abi_dvs_stat_cfg cfg IMGU_ABI_PAD;
    struct imgu_abi_dvs_stat_stripe_data stripe_data IMGU_ABI_PAD;
};

/******************* imgu_abi_stats_lace *******************/

#define IMGU_ABI_LACE_STAT_REGS_PER_SET			320
#define IMGU_ABI_LACE_STAT_MAX_OPERATIONS		41

struct imgu_abi_lace_stat_stats_regs {
    u8 bin[4];					/* the bins 0-3 */
};

struct imgu_abi_lace_stat_hist_single_set {
    struct imgu_abi_lace_stat_stats_regs
        lace_hist_set[IMGU_ABI_LACE_STAT_REGS_PER_SET] IMGU_ABI_PAD;
};

struct imgu_abi_lace_stat_hist_vec {
    struct imgu_abi_lace_stat_hist_single_set
        lace_hist_output[IMGU_ABI_LACE_STAT_MAX_OPERATIONS] IMGU_ABI_PAD;
};

struct imgu_abi_lace_stat_gbl_cfg {
    u32 lh_mode:3;
    u32 __reserved:3;
    u32 y_ds_mode:2;
    u32 uv_ds_mode_unsupported:1;
    u32 uv_input_unsupported:1;
    u32 __reserved1:10;
    u32 rst_loc_hist:1;
    u32 done_rst_loc_hist:1;
    u32 __reserved2:10;
};

struct imgu_abi_lace_stat_y_grd_hor_cfg {
    u32 grid_width:6;
    u32 __reserved:10;
    u32 block_width:4;
    u32 __reserved1:12;
};

struct imgu_abi_lace_stat_y_grd_hor_roi {
    u32 x_start:12;
    u32 __reserved:4;
    u32 x_end:12;
    u32 __reserved1:4;
};

struct imgu_abi_lace_stat_uv_grd_hor_cfg {
    u32 not_supported;
};

struct imgu_abi_lace_stat_uv_grd_hor_roi {
    u32 not_supported;
};

struct imgu_abi_lace_stat_grd_vrt_cfg {
    u32 __reserved:8;
    u32 grid_h:6;
    u32 __reserved1:6;
    u32 block_h:4;
    u32 grid_h_per_slice:7;
    u32 __reserved2:1;
};

struct imgu_abi_lace_stat_grd_vrt_roi {
    u32 y_start:12;
    u32 __reserved:4;
    u32 y_end:12;
    u32 __reserved1:4;
};

struct imgu_abi_lace_stat_cfg {
    struct imgu_abi_lace_stat_gbl_cfg lace_stat_gbl_cfg;
    struct imgu_abi_lace_stat_y_grd_hor_cfg lace_stat_y_grd_hor_cfg;
    struct imgu_abi_lace_stat_y_grd_hor_roi lace_stat_y_grd_hor_roi;
    struct imgu_abi_lace_stat_uv_grd_hor_cfg lace_stat_uv_grd_hor_cfg;
    struct imgu_abi_lace_stat_uv_grd_hor_roi lace_stat_uv_grd_hor_roi;
    struct imgu_abi_lace_stat_grd_vrt_cfg lace_stat_grd_vrt_cfg;
    struct imgu_abi_lace_stat_grd_vrt_roi lace_stat_grd_vrt_roi;
};

struct imgu_abi_stats_lace {
    struct imgu_abi_lace_stat_hist_vec lace_hist_vec IMGU_ABI_PAD;
    struct imgu_abi_lace_stat_cfg lace_stat_cfg IMGU_ABI_PAD;
};

#endif /* PSL_IPU3_IPU3_INTERFACE_H_ */
