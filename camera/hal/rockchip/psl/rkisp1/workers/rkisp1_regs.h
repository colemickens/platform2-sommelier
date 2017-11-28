/*
 * Rockchip isp1 driver
 *
 * Copyright (C) 2017 Rockchip Electronics Co., Ltd.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _RKISP1_REGS_H
#define _RKISP1_REGS_H
/* #include "common.h" */
/* #include "rkisp1.h" */

/* ISP_CTRL */
#define CIF_ISP_CTRL_ISP_ENABLE                 BIT(0)
#define CIF_ISP_CTRL_ISP_MODE_RAW_PICT          (0 << 1)
#define CIF_ISP_CTRL_ISP_MODE_ITU656            (1 << 1)
#define CIF_ISP_CTRL_ISP_MODE_ITU601            (2 << 1)
#define CIF_ISP_CTRL_ISP_MODE_BAYER_ITU601      (3 << 1)
#define CIF_ISP_CTRL_ISP_MODE_DATA_MODE         (4 << 1)
#define CIF_ISP_CTRL_ISP_MODE_BAYER_ITU656      (5 << 1)
#define CIF_ISP_CTRL_ISP_MODE_RAW_PICT_ITU656   (6 << 1)
#define CIF_ISP_CTRL_ISP_INFORM_ENABLE          BIT(4)
#define CIF_ISP_CTRL_ISP_GAMMA_IN_ENA           BIT(6)
#define CIF_ISP_CTRL_ISP_AWB_ENA                BIT(7)
#define CIF_ISP_CTRL_ISP_CFG_UPD_PERMANENT      BIT(8)
#define CIF_ISP_CTRL_ISP_CFG_UPD                BIT(9)
#define CIF_ISP_CTRL_ISP_GEN_CFG_UPD            BIT(10)
#define CIF_ISP_CTRL_ISP_GAMMA_OUT_ENA          BIT(11)
#define CIF_ISP_CTRL_ISP_FLASH_MODE_ENA         BIT(12)
#define CIF_ISP_CTRL_ISP_CSM_Y_FULL_ENA         BIT(13)
#define CIF_ISP_CTRL_ISP_CSM_C_FULL_ENA         BIT(14)

/* ISP_ACQ_PROP */
#define CIF_ISP_ACQ_PROP_POS_EDGE               BIT(0)
#define CIF_ISP_ACQ_PROP_HSYNC_LOW              BIT(1)
#define CIF_ISP_ACQ_PROP_VSYNC_LOW              BIT(2)
#define CIF_ISP_ACQ_PROP_BAYER_PAT_RGGB         (0 << 3)
#define CIF_ISP_ACQ_PROP_BAYER_PAT_GRBG         (1 << 3)
#define CIF_ISP_ACQ_PROP_BAYER_PAT_GBRG         (2 << 3)
#define CIF_ISP_ACQ_PROP_BAYER_PAT_BGGR         (3 << 3)
#define CIF_ISP_ACQ_PROP_BAYER_PAT(pat)         ((pat) << 3)
#define CIF_ISP_ACQ_PROP_YCBYCR                 (0 << 7)
#define CIF_ISP_ACQ_PROP_YCRYCB                 (1 << 7)
#define CIF_ISP_ACQ_PROP_CBYCRY                 (2 << 7)
#define CIF_ISP_ACQ_PROP_CRYCBY                 (3 << 7)
#define CIF_ISP_ACQ_PROP_FIELD_SEL_ALL          (0 << 9)
#define CIF_ISP_ACQ_PROP_FIELD_SEL_EVEN         (1 << 9)
#define CIF_ISP_ACQ_PROP_FIELD_SEL_ODD          (2 << 9)
#define CIF_ISP_ACQ_PROP_IN_SEL_12B             (0 << 12)
#define CIF_ISP_ACQ_PROP_IN_SEL_10B_ZERO        (1 << 12)
#define CIF_ISP_ACQ_PROP_IN_SEL_10B_MSB         (2 << 12)
#define CIF_ISP_ACQ_PROP_IN_SEL_8B_ZERO         (3 << 12)
#define CIF_ISP_ACQ_PROP_IN_SEL_8B_MSB          (4 << 12)

/* VI_DPCL */
#define CIF_VI_DPCL_DMA_JPEG                    (0 << 0)
#define CIF_VI_DPCL_MP_MUX_MRSZ_MI              (1 << 0)
#define CIF_VI_DPCL_MP_MUX_MRSZ_JPEG            (2 << 0)
#define CIF_VI_DPCL_CHAN_MODE_MP                (1 << 2)
#define CIF_VI_DPCL_CHAN_MODE_SP                (2 << 2)
#define CIF_VI_DPCL_CHAN_MODE_MPSP              (3 << 2)
#define CIF_VI_DPCL_DMA_SW_SPMUX                (0 << 4)
#define CIF_VI_DPCL_DMA_SW_SI                   (1 << 4)
#define CIF_VI_DPCL_DMA_SW_IE                   (2 << 4)
#define CIF_VI_DPCL_DMA_SW_JPEG                 (3 << 4)
#define CIF_VI_DPCL_DMA_SW_ISP                  (4 << 4)
#define CIF_VI_DPCL_IF_SEL_PARALLEL             (0 << 8)
#define CIF_VI_DPCL_IF_SEL_SMIA                 (1 << 8)
#define CIF_VI_DPCL_IF_SEL_MIPI                 (2 << 8)
#define CIF_VI_DPCL_DMA_IE_MUX_DMA              BIT(10)
#define CIF_VI_DPCL_DMA_SP_MUX_DMA              BIT(11)

/* ISP_IMSC - ISP_MIS - ISP_RIS - ISP_ICR - ISP_ISR */
#define CIF_ISP_OFF                             BIT(0)
#define CIF_ISP_FRAME                           BIT(1)
#define CIF_ISP_DATA_LOSS                       BIT(2)
#define CIF_ISP_PIC_SIZE_ERROR                  BIT(3)
#define CIF_ISP_AWB_DONE                        BIT(4)
#define CIF_ISP_FRAME_IN                        BIT(5)
#define CIF_ISP_V_START                         BIT(6)
#define CIF_ISP_H_START                         BIT(7)
#define CIF_ISP_FLASH_ON                        BIT(8)
#define CIF_ISP_FLASH_OFF                       BIT(9)
#define CIF_ISP_SHUTTER_ON                      BIT(10)
#define CIF_ISP_SHUTTER_OFF                     BIT(11)
#define CIF_ISP_AFM_SUM_OF                      BIT(12)
#define CIF_ISP_AFM_LUM_OF                      BIT(13)
#define CIF_ISP_AFM_FIN                         BIT(14)
#define CIF_ISP_HIST_MEASURE_RDY                BIT(15)
#define CIF_ISP_FLASH_CAP                       BIT(17)
#define CIF_ISP_EXP_END                         BIT(18)
#define CIF_ISP_VSM_END                         BIT(19)

/* ISP_ERR */
#define CIF_ISP_ERR_INFORM_SIZE                 BIT(0)
#define CIF_ISP_ERR_IS_SIZE                     BIT(1)
#define CIF_ISP_ERR_OUTFORM_SIZE                BIT(2)

/* MI_CTRL */
#define CIF_MI_CTRL_MP_ENABLE                   (1 << 0)
#define CIF_MI_CTRL_SP_ENABLE                   (2 << 0)
#define CIF_MI_CTRL_JPEG_ENABLE                 (4 << 0)
#define CIF_MI_CTRL_RAW_ENABLE                  (8 << 0)
#define CIF_MI_CTRL_HFLIP                       BIT(4)
#define CIF_MI_CTRL_VFLIP                       BIT(5)
#define CIF_MI_CTRL_ROT                         BIT(6)
#define CIF_MI_BYTE_SWAP                        BIT(7)
#define CIF_MI_SP_Y_FULL_YUV2RGB                BIT(8)
#define CIF_MI_SP_CBCR_FULL_YUV2RGB             BIT(9)
#define CIF_MI_SP_422NONCOSITEED                BIT(10)
#define CIF_MI_MP_PINGPONG_ENABEL               BIT(11)
#define CIF_MI_SP_PINGPONG_ENABEL               BIT(12)
#define CIF_MI_MP_AUTOUPDATE_ENABLE             BIT(13)
#define CIF_MI_SP_AUTOUPDATE_ENABLE             BIT(14)
#define CIF_MI_LAST_PIXEL_SIG_ENABLE            BIT(15)
#define CIF_MI_CTRL_BURST_LEN_LUM_16            (0 << 16)
#define CIF_MI_CTRL_BURST_LEN_LUM_32            (1 << 16)
#define CIF_MI_CTRL_BURST_LEN_LUM_64            (2 << 16)
#define CIF_MI_CTRL_BURST_LEN_CHROM_16          (0 << 18)
#define CIF_MI_CTRL_BURST_LEN_CHROM_32          (1 << 18)
#define CIF_MI_CTRL_BURST_LEN_CHROM_64          (2 << 18)
#define CIF_MI_CTRL_INIT_BASE_EN                BIT(20)
#define CIF_MI_CTRL_INIT_OFFSET_EN              BIT(21)
#define MI_CTRL_MP_WRITE_YUV_PLA_OR_RAW8        (0 << 22)
#define MI_CTRL_MP_WRITE_YUV_SPLA               (1 << 22)
#define MI_CTRL_MP_WRITE_YUVINT                 (2 << 22)
#define MI_CTRL_MP_WRITE_RAW12                  (2 << 22)
#define MI_CTRL_SP_WRITE_PLA                    (0 << 24)
#define MI_CTRL_SP_WRITE_SPLA                   (1 << 24)
#define MI_CTRL_SP_WRITE_INT                    (2 << 24)
#define MI_CTRL_SP_INPUT_YUV400                 (0 << 26)
#define MI_CTRL_SP_INPUT_YUV420                 (1 << 26)
#define MI_CTRL_SP_INPUT_YUV422                 (2 << 26)
#define MI_CTRL_SP_INPUT_YUV444                 (3 << 26)
#define MI_CTRL_SP_OUTPUT_YUV400                (0 << 28)
#define MI_CTRL_SP_OUTPUT_YUV420                (1 << 28)
#define MI_CTRL_SP_OUTPUT_YUV422                (2 << 28)
#define MI_CTRL_SP_OUTPUT_YUV444                (3 << 28)
#define MI_CTRL_SP_OUTPUT_RGB565                (4 << 28)
#define MI_CTRL_SP_OUTPUT_RGB666                (5 << 28)
#define MI_CTRL_SP_OUTPUT_RGB888                (6 << 28)

#define MI_CTRL_MP_FMT_MASK                     GENMASK(23, 22)
#define MI_CTRL_SP_FMT_MASK                     GENMASK(30, 24)

/* MI_INIT */
#define CIF_MI_INIT_SKIP                        BIT(2)
#define CIF_MI_INIT_SOFT_UPD                    BIT(4)

/* RSZ_CTRL */
#define CIF_RSZ_CTRL_SCALE_HY_ENABLE            BIT(0)
#define CIF_RSZ_CTRL_SCALE_HC_ENABLE            BIT(1)
#define CIF_RSZ_CTRL_SCALE_VY_ENABLE            BIT(2)
#define CIF_RSZ_CTRL_SCALE_VC_ENABLE            BIT(3)
#define CIF_RSZ_CTRL_SCALE_HY_UP                BIT(4)
#define CIF_RSZ_CTRL_SCALE_HC_UP                BIT(5)
#define CIF_RSZ_CTRL_SCALE_VY_UP                BIT(6)
#define CIF_RSZ_CTRL_SCALE_VC_UP                BIT(7)
#define CIF_RSZ_CTRL_CFG_UPD                    BIT(8)
#define CIF_RSZ_CTRL_CFG_UPD_AUTO               BIT(9)
#define CIF_RSZ_SCALER_FACTOR                   BIT(16)

/* MI_IMSC - MI_MIS - MI_RIS - MI_ICR - MI_ISR */
#define CIF_MI_MP_FRAME                         BIT(0)
#define CIF_MI_SP_FRAME                         BIT(1)
#define CIF_MI_MBLK_LINE                        BIT(2)
#define CIF_MI_FILL_MP_Y                        BIT(3)
#define CIF_MI_WRAP_MP_Y                        BIT(4)
#define CIF_MI_WRAP_MP_CB                       BIT(5)
#define CIF_MI_WRAP_MP_CR                       BIT(6)
#define CIF_MI_WRAP_SP_Y                        BIT(7)
#define CIF_MI_WRAP_SP_CB                       BIT(8)
#define CIF_MI_WRAP_SP_CR                       BIT(9)
#define CIF_MI_DMA_READY                        BIT(11)

/* MI_STATUS */
#define CIF_MI_STATUS_MP_Y_FIFO_FULL            BIT(0)
#define CIF_MI_STATUS_SP_Y_FIFO_FULL            BIT(4)

/* MI_DMA_CTRL */
#define CIF_MI_DMA_CTRL_BURST_LEN_LUM_16        (0 << 0)
#define CIF_MI_DMA_CTRL_BURST_LEN_LUM_32        (1 << 0)
#define CIF_MI_DMA_CTRL_BURST_LEN_LUM_64        (2 << 0)
#define CIF_MI_DMA_CTRL_BURST_LEN_CHROM_16      (0 << 2)
#define CIF_MI_DMA_CTRL_BURST_LEN_CHROM_32      (1 << 2)
#define CIF_MI_DMA_CTRL_BURST_LEN_CHROM_64      (2 << 2)
#define CIF_MI_DMA_CTRL_READ_FMT_PLANAR         (0 << 4)
#define CIF_MI_DMA_CTRL_READ_FMT_SPLANAR        (1 << 4)
#define CIF_MI_DMA_CTRL_FMT_YUV400              (0 << 6)
#define CIF_MI_DMA_CTRL_FMT_YUV420              (1 << 6)
#define CIF_MI_DMA_CTRL_READ_FMT_PACKED         (2 << 4)
#define CIF_MI_DMA_CTRL_FMT_YUV422              (2 << 6)
#define CIF_MI_DMA_CTRL_FMT_YUV444              (3 << 6)
#define CIF_MI_DMA_CTRL_BYTE_SWAP               BIT(8)
#define CIF_MI_DMA_CTRL_CONTINUOUS_ENA          BIT(9)
#define CIF_MI_DMA_CTRL_RGB_BAYER_NO            (0 << 12)
#define CIF_MI_DMA_CTRL_RGB_BAYER_8BIT          (1 << 12)
#define CIF_MI_DMA_CTRL_RGB_BAYER_16BIT         (2 << 12)
/* MI_DMA_START */
#define CIF_MI_DMA_START_ENABLE                 BIT(0)
/* MI_XTD_FORMAT_CTRL  */
#define CIF_MI_XTD_FMT_CTRL_MP_CB_CR_SWAP       BIT(0)
#define CIF_MI_XTD_FMT_CTRL_SP_CB_CR_SWAP       BIT(1)
#define CIF_MI_XTD_FMT_CTRL_DMA_CB_CR_SWAP      BIT(2)

/* CCL */
#define CIF_CCL_CIF_CLK_DIS                     BIT(2)
/* ICCL */
#define CIF_ICCL_ISP_CLK                        BIT(0)
#define CIF_ICCL_CP_CLK                         BIT(1)
#define CIF_ICCL_RES_2                          BIT(2)
#define CIF_ICCL_MRSZ_CLK                       BIT(3)
#define CIF_ICCL_SRSZ_CLK                       BIT(4)
#define CIF_ICCL_JPEG_CLK                       BIT(5)
#define CIF_ICCL_MI_CLK                         BIT(6)
#define CIF_ICCL_RES_7                          BIT(7)
#define CIF_ICCL_IE_CLK                         BIT(8)
#define CIF_ICCL_SIMP_CLK                       BIT(9)
#define CIF_ICCL_SMIA_CLK                       BIT(10)
#define CIF_ICCL_MIPI_CLK                       BIT(11)
#define CIF_ICCL_DCROP_CLK                      BIT(12)
/* IRCL */
#define CIF_IRCL_ISP_SW_RST                     BIT(0)
#define CIF_IRCL_CP_SW_RST                      BIT(1)
#define CIF_IRCL_YCS_SW_RST                     BIT(2)
#define CIF_IRCL_MRSZ_SW_RST                    BIT(3)
#define CIF_IRCL_SRSZ_SW_RST                    BIT(4)
#define CIF_IRCL_JPEG_SW_RST                    BIT(5)
#define CIF_IRCL_MI_SW_RST                      BIT(6)
#define CIF_IRCL_CIF_SW_RST                     BIT(7)
#define CIF_IRCL_IE_SW_RST                      BIT(8)
#define CIF_IRCL_SI_SW_RST                      BIT(9)
#define CIF_IRCL_MIPI_SW_RST                    BIT(11)

/* C_PROC_CTR */
#define CIF_C_PROC_CTR_ENABLE                   BIT(0)
#define CIF_C_PROC_YOUT_FULL                    BIT(1)
#define CIF_C_PROC_YIN_FULL                     BIT(2)
#define CIF_C_PROC_COUT_FULL                    BIT(3)
#define CIF_C_PROC_CTRL_RESERVED                0xFFFFFFFE
#define CIF_C_PROC_CONTRAST_RESERVED            0xFFFFFF00
#define CIF_C_PROC_BRIGHTNESS_RESERVED          0xFFFFFF00
#define CIF_C_PROC_HUE_RESERVED                 0xFFFFFF00
#define CIF_C_PROC_SATURATION_RESERVED          0xFFFFFF00
#define CIF_C_PROC_MACC_RESERVED                0xE000E000
#define CIF_C_PROC_TONE_RESERVED                0xF000
/* DUAL_CROP_CTRL */
#define CIF_DUAL_CROP_MP_MODE_BYPASS            (0 << 0)
#define CIF_DUAL_CROP_MP_MODE_YUV               (1 << 0)
#define CIF_DUAL_CROP_MP_MODE_RAW               (2 << 0)
#define CIF_DUAL_CROP_SP_MODE_BYPASS            (0 << 2)
#define CIF_DUAL_CROP_SP_MODE_YUV               (1 << 2)
#define CIF_DUAL_CROP_SP_MODE_RAW               (2 << 2)
#define CIF_DUAL_CROP_CFG_UPD_PERMANENT         BIT(4)
#define CIF_DUAL_CROP_CFG_UPD                   BIT(5)
#define CIF_DUAL_CROP_GEN_CFG_UPD               BIT(6)

/* IMG_EFF_CTRL */
#define CIF_IMG_EFF_CTRL_ENABLE                 BIT(0)
#define CIF_IMG_EFF_CTRL_MODE_BLACKWHITE        (0 << 1)
#define CIF_IMG_EFF_CTRL_MODE_NEGATIVE          (1 << 1)
#define CIF_IMG_EFF_CTRL_MODE_SEPIA             (2 << 1)
#define CIF_IMG_EFF_CTRL_MODE_COLOR_SEL         (3 << 1)
#define CIF_IMG_EFF_CTRL_MODE_EMBOSS            (4 << 1)
#define CIF_IMG_EFF_CTRL_MODE_SKETCH            (5 << 1)
#define CIF_IMG_EFF_CTRL_MODE_SHARPEN           (6 << 1)
#define CIF_IMG_EFF_CTRL_CFG_UPD                BIT(4)
#define CIF_IMG_EFF_CTRL_YCBCR_FULL             BIT(5)

#define CIF_IMG_EFF_CTRL_MODE_BLACKWHITE_SHIFT  0
#define CIF_IMG_EFF_CTRL_MODE_NEGATIVE_SHIFT    1
#define CIF_IMG_EFF_CTRL_MODE_SEPIA_SHIFT       2
#define CIF_IMG_EFF_CTRL_MODE_COLOR_SEL_SHIFT   3
#define CIF_IMG_EFF_CTRL_MODE_EMBOSS_SHIFT      4
#define CIF_IMG_EFF_CTRL_MODE_SKETCH_SHIFT      5
#define CIF_IMG_EFF_CTRL_MODE_SHARPEN_SHIFT     6
#define CIF_IMG_EFF_CTRL_MODE_MASK              0xE

/* IMG_EFF_COLOR_SEL */
#define CIF_IMG_EFF_COLOR_RGB                   0
#define CIF_IMG_EFF_COLOR_B                     (1 << 0)
#define CIF_IMG_EFF_COLOR_G                     (2 << 0)
#define CIF_IMG_EFF_COLOR_GB                    (3 << 0)
#define CIF_IMG_EFF_COLOR_R                     (4 << 0)
#define CIF_IMG_EFF_COLOR_RB                    (5 << 0)
#define CIF_IMG_EFF_COLOR_RG                    (6 << 0)
#define CIF_IMG_EFF_COLOR_RGB2                  (7 << 0)

/* MIPI_CTRL */
#define CIF_MIPI_CTRL_OUTPUT_ENA                BIT(0)
#define CIF_MIPI_CTRL_SHUTDOWNLANES(a)          (((a) & 0xF) << 8)
#define CIF_MIPI_CTRL_NUM_LANES(a)              (((a) & 0x3) << 12)
#define CIF_MIPI_CTRL_ERR_SOT_HS_SKIP           BIT(16)
#define CIF_MIPI_CTRL_ERR_SOT_SYNC_HS_SKIP      BIT(17)
#define CIF_MIPI_CTRL_CLOCKLANE_ENA             BIT(18)

/* MIPI_DATA_SEL */
#define CIF_MIPI_DATA_SEL_VC(a)                 (((a) & 0x3) << 6)
#define CIF_MIPI_DATA_SEL_DT(a)                 (((a) & 0x3F) << 0)
/* MIPI DATA_TYPE */
#define CIF_CSI2_DT_YUV420_8b                   0x18
#define CIF_CSI2_DT_YUV420_10b                  0x19
#define CIF_CSI2_DT_YUV422_8b                   0x1E
#define CIF_CSI2_DT_YUV422_10b                  0x1F
#define CIF_CSI2_DT_RGB565                      0x22
#define CIF_CSI2_DT_RGB666                      0x23
#define CIF_CSI2_DT_RGB888                      0x24
#define CIF_CSI2_DT_RAW8                        0x2A
#define CIF_CSI2_DT_RAW10                       0x2B
#define CIF_CSI2_DT_RAW12                       0x2C

/* MIPI_IMSC, MIPI_RIS, MIPI_MIS, MIPI_ICR, MIPI_ISR */
#define CIF_MIPI_SYNC_FIFO_OVFLW(a)             (((a) & 0xF) << 0)
#define CIF_MIPI_ERR_SOT(a)                     (((a) & 0xF) << 4)
#define CIF_MIPI_ERR_SOT_SYNC(a)                (((a) & 0xF) << 8)
#define CIF_MIPI_ERR_EOT_SYNC(a)                (((a) & 0xF) << 12)
#define CIF_MIPI_ERR_CTRL(a)                    (((a) & 0xF) << 16)
#define CIF_MIPI_ERR_PROTOCOL                   BIT(20)
#define CIF_MIPI_ERR_ECC1                       BIT(21)
#define CIF_MIPI_ERR_ECC2                       BIT(22)
#define CIF_MIPI_ERR_CS                         BIT(23)
#define CIF_MIPI_FRAME_END                      BIT(24)
#define CIF_MIPI_ADD_DATA_OVFLW                 BIT(25)
#define CIF_MIPI_ADD_DATA_WATER_MARK            BIT(26)

#define CIF_MIPI_ERR_CSI  (CIF_MIPI_ERR_PROTOCOL | \
    CIF_MIPI_ERR_ECC1 | \
    CIF_MIPI_ERR_ECC2 | \
    CIF_MIPI_ERR_CS)

#define CIF_MIPI_ERR_DPHY  (CIF_MIPI_ERR_SOT(3) | \
    CIF_MIPI_ERR_SOT_SYNC(3) | \
    CIF_MIPI_ERR_EOT_SYNC(3) | \
    CIF_MIPI_ERR_CTRL(3))

/* SUPER_IMPOSE */
#define CIF_SUPER_IMP_CTRL_NORMAL_MODE          BIT(0)
#define CIF_SUPER_IMP_CTRL_REF_IMG_MEM          BIT(1)
#define CIF_SUPER_IMP_CTRL_TRANSP_DIS           BIT(2)

/* ISP HISTOGRAM CALCULATION : ISP_HIST_PROP */
#define CIF_ISP_HIST_PROP_MODE_DIS              (0 << 0)
#define CIF_ISP_HIST_PROP_MODE_RGB              (1 << 0)
#define CIF_ISP_HIST_PROP_MODE_RED              (2 << 0)
#define CIF_ISP_HIST_PROP_MODE_GREEN            (3 << 0)
#define CIF_ISP_HIST_PROP_MODE_BLUE             (4 << 0)
#define CIF_ISP_HIST_PROP_MODE_LUM              (5 << 0)
#define CIF_ISP_HIST_PROP_MODE_MASK             0x7
#define CIF_ISP_HIST_PREDIV_SET(x)              (((x) & 0x7F) << 3)
#define CIF_ISP_HIST_WEIGHT_SET(v0, v1, v2, v3) \
                     (((v0) & 0x1F) | (((v1) & 0x1F) << 8)  |\
                     (((v2) & 0x1F) << 16) | \
                     (((v3) & 0x1F) << 24))

#define CIF_ISP_HIST_WINDOW_OFFSET_RESERVED     0xFFFFF000
#define CIF_ISP_HIST_WINDOW_SIZE_RESERVED       0xFFFFF800
#define CIF_ISP_HIST_WEIGHT_RESERVED            0xE0E0E0E0
#define CIF_ISP_MAX_HIST_PREDIVIDER             0x0000007F
#define CIF_ISP_HIST_ROW_NUM                    5
#define CIF_ISP_HIST_COLUMN_NUM                 5

/* AUTO FOCUS MEASUREMENT:  ISP_AFM_CTRL */
#define ISP_AFM_CTRL_ENABLE                     BIT(0)

/* SHUTTER CONTROL */
#define CIF_ISP_SH_CTRL_SH_ENA                  BIT(0)
#define CIF_ISP_SH_CTRL_REP_EN                  BIT(1)
#define CIF_ISP_SH_CTRL_SRC_SH_TRIG             BIT(2)
#define CIF_ISP_SH_CTRL_EDGE_POS                BIT(3)
#define CIF_ISP_SH_CTRL_POL_LOW                 BIT(4)

/* FLASH MODULE */
/* ISP_FLASH_CMD */
#define CIF_FLASH_CMD_PRELIGHT_ON               BIT(0)
#define CIF_FLASH_CMD_FLASH_ON                  BIT(1)
#define CIF_FLASH_CMD_PRE_FLASH_ON              BIT(2)
/* ISP_FLASH_CONFIG */
#define CIF_FLASH_CONFIG_PRELIGHT_END           BIT(0)
#define CIF_FLASH_CONFIG_VSYNC_POS              BIT(1)
#define CIF_FLASH_CONFIG_PRELIGHT_LOW           BIT(2)
#define CIF_FLASH_CONFIG_SRC_FL_TRIG            BIT(3)
#define CIF_FLASH_CONFIG_DELAY(a)               (((a) & 0xF) << 4)

/* Demosaic:  ISP_DEMOSAIC */
#define CIF_ISP_DEMOSAIC_BYPASS                 BIT(10)
#define CIF_ISP_DEMOSAIC_TH(x)                  ((x) & 0xFF)

/* AWB */
/* ISP_AWB_PROP */
#define CIF_ISP_AWB_YMAX_CMP_EN                 BIT(2)
#define CIFISP_AWB_YMAX_READ(x)                 (((x) >> 2) & 1)
#define CIF_ISP_AWB_MODE_RGB_EN                 ((1 << 31) | (0x2 << 0))
#define CIF_ISP_AWB_MODE_YCBCR_EN               ((0 << 31) | (0x2 << 0))
#define CIF_ISP_AWB_MODE_YCBCR_EN               ((0 << 31) | (0x2 << 0))
#define CIF_ISP_AWB_MODE_MASK_NONE              0xFFFFFFFC
#define CIF_ISP_AWB_MODE_READ(x)                ((x) & 3)
/* ISP_AWB_GAIN_RB, ISP_AWB_GAIN_G  */
#define CIF_ISP_AWB_GAIN_R_SET(x)               (((x) & 0x3FF) << 16)
#define CIF_ISP_AWB_GAIN_R_READ(x)              (((x) >> 16) & 0x3FF)
#define CIF_ISP_AWB_GAIN_B_SET(x)               ((x) & 0x3FFF)
#define CIF_ISP_AWB_GAIN_B_READ(x)              ((x) & 0x3FFF)
/* ISP_AWB_REF */
#define CIF_ISP_AWB_REF_CR_SET(x)               (((x) & 0xFF) << 8)
#define CIF_ISP_AWB_REF_CR_READ(x)              (((x) >> 8) & 0xFF)
#define CIF_ISP_AWB_REF_CB_READ(x)              ((x) & 0xFF)
/* ISP_AWB_THRESH */
#define CIF_ISP_AWB_MAX_CS_SET(x)               (((x) & 0xFF) << 8)
#define CIF_ISP_AWB_MAX_CS_READ(x)              (((x) >> 8) & 0xFF)
#define CIF_ISP_AWB_MIN_C_READ(x)               ((x) & 0xFF)
#define CIF_ISP_AWB_MIN_Y_SET(x)                (((x) & 0xFF) << 16)
#define CIF_ISP_AWB_MIN_Y_READ(x)               (((x) >> 16) & 0xFF)
#define CIF_ISP_AWB_MAX_Y_SET(x)                (((x) & 0xFF) << 24)
#define CIF_ISP_AWB_MAX_Y_READ(x)               (((x) >> 24) & 0xFF)
/* ISP_AWB_MEAN */
#define CIF_ISP_AWB_GET_MEAN_CR_R(x)            ((x) & 0xFF)
#define CIF_ISP_AWB_GET_MEAN_CB_B(x)            (((x) >> 8) & 0xFF)
#define CIF_ISP_AWB_GET_MEAN_Y_G(x)             (((x) >> 16) & 0xFF)
/* ISP_AWB_WHITE_CNT */
#define CIF_ISP_AWB_GET_PIXEL_CNT(x)            ((x) & 0x3FFFFFF)

#define CIF_ISP_AWB_GAINS_MAX_VAL               0x000003FF
#define CIF_ISP_AWB_WINDOW_OFFSET_MAX           0x00000FFF
#define CIF_ISP_AWB_WINDOW_MAX_SIZE             0x00001FFF
#define CIF_ISP_AWB_CBCR_MAX_REF                0x000000FF
#define CIF_ISP_AWB_THRES_MAX_YC                0x000000FF

/* AE */
/* ISP_EXP_CTRL */
#define CIF_ISP_EXP_ENA                         BIT(0)
#define CIF_ISP_EXP_CTRL_AUTOSTOP               BIT(1)
/*
 *'1' luminance calculation according to  Y=(R+G+B) x 0.332 (85/256)
 *'0' luminance calculation according to Y=16+0.25R+0.5G+0.1094B
 */
#define CIF_ISP_EXP_CTRL_MEASMODE_1             BIT(31)

/* ISP_EXP_H_SIZE */
#define CIF_ISP_EXP_H_SIZE_SET(x)               ((x) & 0x7FF)
#define CIF_ISP_EXP_HEIGHT_MASK                 0x000007FF
/* ISP_EXP_V_SIZE : vertical size must be a multiple of 2). */
#define CIF_ISP_EXP_V_SIZE_SET(x)               ((x) & 0x7FE)

/* ISP_EXP_H_OFFSET */
#define CIF_ISP_EXP_H_OFFSET_SET(x)             ((x) & 0x1FFF)
#define CIF_ISP_EXP_MAX_HOFFS                   2424
/* ISP_EXP_V_OFFSET */
#define CIF_ISP_EXP_V_OFFSET_SET(x)             ((x) & 0x1FFF)
#define CIF_ISP_EXP_MAX_VOFFS                   1806

#define CIF_ISP_EXP_ROW_NUM                     5
#define CIF_ISP_EXP_COLUMN_NUM                  5
#define CIF_ISP_EXP_NUM_LUMA_REGS \
    (CIF_ISP_EXP_ROW_NUM * CIF_ISP_EXP_COLUMN_NUM)
#define CIF_ISP_EXP_BLOCK_MAX_HSIZE             516
#define CIF_ISP_EXP_BLOCK_MIN_HSIZE             35
#define CIF_ISP_EXP_BLOCK_MAX_VSIZE             390
#define CIF_ISP_EXP_BLOCK_MIN_VSIZE             28
#define CIF_ISP_EXP_MAX_HSIZE                   \
    (CIF_ISP_EXP_BLOCK_MAX_HSIZE * CIF_ISP_EXP_COLUMN_NUM + 1)
#define CIF_ISP_EXP_MIN_HSIZE                   \
    (CIF_ISP_EXP_BLOCK_MIN_HSIZE * CIF_ISP_EXP_COLUMN_NUM + 1)
#define CIF_ISP_EXP_MAX_VSIZE                   \
    (CIF_ISP_EXP_BLOCK_MAX_VSIZE * CIF_ISP_EXP_ROW_NUM + 1)
#define CIF_ISP_EXP_MIN_VSIZE                   \
    (CIF_ISP_EXP_BLOCK_MIN_VSIZE * CIF_ISP_EXP_ROW_NUM + 1)

/* LSC: ISP_LSC_CTRL */
#define CIF_ISP_LSC_CTRL_ENA                    BIT(0)
#define CIF_ISP_LSC_SECT_SIZE_RESERVED          0xFC00FC00
#define CIF_ISP_LSC_GRAD_RESERVED               0xF000F000
#define CIF_ISP_LSC_SAMPLE_RESERVED             0xF000F000
#define CIF_ISP_LSC_SECTORS_MAX                 16
#define CIF_ISP_LSC_TABLE_DATA(v0, v1)     \
    (((v0) & 0xFFF) | (((v1) & 0xFFF) << 12))
#define CIF_ISP_LSC_SECT_SIZE(v0, v1)      \
    (((v0) & 0xFFF) | (((v1) & 0xFFF) << 16))
#define CIF_ISP_LSC_GRAD_SIZE(v0, v1)      \
    (((v0) & 0xFFF) | (((v1) & 0xFFF) << 16))

/* LSC: ISP_LSC_TABLE_SEL */
#define CIF_ISP_LSC_TABLE_0                     0
#define CIF_ISP_LSC_TABLE_1                     1

/* LSC: ISP_LSC_STATUS */
#define CIF_ISP_LSC_ACTIVE_TABLE                BIT(1)
#define CIF_ISP_LSC_TABLE_ADDRESS_0             0
#define CIF_ISP_LSC_TABLE_ADDRESS_153           153

/* FLT */
/* ISP_FILT_MODE */
#define CIF_ISP_FLT_ENA                         BIT(0)

/*
 * 0: green filter static mode (active filter factor = FILT_FAC_MID)
 * 1: dynamic noise reduction/sharpen Default
 */
#define CIF_ISP_FLT_MODE_DNR                    BIT(1)
#define CIF_ISP_FLT_MODE_MAX                    1
#define CIF_ISP_FLT_CHROMA_V_MODE(x)            (((x) & 0x3) << 4)
#define CIF_ISP_FLT_CHROMA_H_MODE(x)            (((x) & 0x3) << 6)
#define CIF_ISP_FLT_CHROMA_MODE_MAX             3
#define CIF_ISP_FLT_GREEN_STAGE1(x)             (((x) & 0xF) << 8)
#define CIF_ISP_FLT_GREEN_STAGE1_MAX            8
#define CIF_ISP_FLT_THREAD_RESERVED             0xFFFFFC00
#define CIF_ISP_FLT_FAC_RESERVED                0xFFFFFFC0
#define CIF_ISP_FLT_LUM_WEIGHT_RESERVED         0xFFF80000

#define CIF_ISP_CTK_COEFF_RESERVED              0xFFFFF800
#define CIF_ISP_XTALK_OFFSET_RESERVED           0xFFFFF000

/* GOC */
#define CIF_ISP_GAMMA_OUT_MODE_EQU              BIT(0)
#define CIF_ISP_GOC_MODE_MAX                    1
#define CIF_ISP_GOC_RESERVED                    0xFFFFF800
/* ISP_CTRL BIT 11*/
#define CIF_ISP_CTRL_ISP_GAMMA_OUT_ENA_READ(x)  (((x) >> 11) & 1)

/* DPCC */
/* ISP_DPCC_MODE */
#define CIF_ISP_DPCC_ENA                        BIT(0)
#define CIF_ISP_DPCC_MODE_MAX                   0x07
#define CIF_ISP_DPCC_OUTPUTMODE_MAX             0x0F
#define CIF_ISP_DPCC_SETUSE_MAX                 0x0F
#define CIF_ISP_DPCC_METHODS_SET_RESERVED       0xFFFFE000
#define CIF_ISP_DPCC_LINE_THRESH_RESERVED       0xFFFF0000
#define CIF_ISP_DPCC_LINE_MAD_FAC_RESERVED      0xFFFFC0C0
#define CIF_ISP_DPCC_PG_FAC_RESERVED            0xFFFFC0C0
#define CIF_ISP_DPCC_RND_THRESH_RESERVED        0xFFFF0000
#define CIF_ISP_DPCC_RG_FAC_RESERVED            0xFFFFC0C0
#define CIF_ISP_DPCC_RO_LIMIT_RESERVED          0xFFFFF000
#define CIF_ISP_DPCC_RND_OFFS_RESERVED          0xFFFFF000

/* BLS */
/* ISP_BLS_CTRL */
#define CIF_ISP_BLS_ENA                         BIT(0)
#define CIF_ISP_BLS_MODE_MEASURED               BIT(1)
#define CIF_ISP_BLS_MODE_FIXED                  0
#define CIF_ISP_BLS_WINDOW_1                    (1 << 2)
#define CIF_ISP_BLS_WINDOW_2                    (2 << 2)

/* GAMMA-IN */
#define CIFISP_DEGAMMA_X_RESERVED               \
    ((1 << 31) | (1 << 27) | (1 << 23) | (1 << 19) |\
    (1 << 15) | (1 << 11) | (1 << 7) | (1 << 3))
#define CIFISP_DEGAMMA_Y_RESERVED               0xFFFFF000

/* AFM */
#define CIF_ISP_AFM_ENA                         BIT(0)
#define CIF_ISP_AFM_THRES_RESERVED              0xFFFF0000
#define CIF_ISP_AFM_VAR_SHIFT_RESERVED          0xFFF8FFF8
#define CIF_ISP_AFM_WINDOW_X_RESERVED           0xE000
#define CIF_ISP_AFM_WINDOW_Y_RESERVED           0xF000
#define CIF_ISP_AFM_WINDOW_X_MIN                0x5
#define CIF_ISP_AFM_WINDOW_Y_MIN                0x2
#define CIF_ISP_AFM_WINDOW_X(x)                 (((x) & 0x1FFF) << 16)
#define CIF_ISP_AFM_WINDOW_Y(x)                 ((x) & 0x1FFF)

/* DPF */
#define CIF_ISP_DPF_MODE_EN                     BIT(0)
#define CIF_ISP_DPF_MODE_B_FLT_DIS              BIT(1)
#define CIF_ISP_DPF_MODE_GB_FLT_DIS             BIT(2)
#define CIF_ISP_DPF_MODE_GR_FLT_DIS             BIT(3)
#define CIF_ISP_DPF_MODE_R_FLT_DIS              BIT(4)
#define CIF_ISP_DPF_MODE_RB_FLTSIZE_9x9         BIT(5)
#define CIF_ISP_DPF_MODE_NLL_SEGMENTATION       BIT(6)
#define CIF_ISP_DPF_MODE_AWB_GAIN_COMP          BIT(7)
#define CIF_ISP_DPF_MODE_LSC_GAIN_COMP          BIT(8)
#define CIF_ISP_DPF_MODE_USE_NF_GAIN            BIT(9)
#define CIF_ISP_DPF_NF_GAIN_RESERVED            0xFFFFF000
#define CIF_ISP_DPF_SPATIAL_COEFF_MAX           0x1F
#define CIF_ISP_DPF_NLL_COEFF_N_MAX             0x3FF

#endif /* _RKISP1_REGS_H */
