/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_MTK_P1_METABUF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_MTK_P1_METABUF_H_

#include <stdint.h> /* uint32_t, uint8_t ... etc */
#include <string.h> /* memset */

/**
 * This header file is a public platform dependent definitions.
 */

/* revision info, Major ver: Bits[16:31], minor ver: bits[0:15] */
#define MTK_P1_MEM_LAYOUT_VER 0x00000002 /* beta: v0.2 */

/* describes metadata identity */
#define MTK_P1_METABUF_UNDEF 0

#define MTK_P1_METABUF_TUNING 0x1D010000 /* identity of meta device TUNING */
#define MTK_P1_METABUF_META1 0x1D020000  /* identity of meta device META1  */
#define MTK_P1_METABUF_META2 0x1D030000  /* identity of meta device META2  */

/* describes metadata size in bytes */
#define MTK_P1_SIZE_TUNING 106596 /* size of meta device TUNING */
#define MTK_P1_SIZE_META1 1135456 /* size of meta device META1  */
#define MTK_P1_SIZE_META2 655424  /* size of meta device META2  */

/* guard token */
#define MTK_P1_GUARDTOKEN_S 0xC0DE450F /* code for sof */
#define MTK_P1_GUARDTOKEN_E 0xC0DE4E0F /* code for eof */

/* -------------------------------------------------------------------------- */
/* memory layout structures                                                   */
/* -------------------------------------------------------------------------- */
/**
 * Mediatek P1 driver input video device meta buffer: TUNING
 */
struct mtk_p1_metabuf_tuning {
  uint8_t data[MTK_P1_SIZE_TUNING];
} __attribute__((packed));

/**
 * Mediatek P1 driver output video device meta buffer: META1
 */
struct mtk_p1_metabuf_meta1 {
  uint8_t data[MTK_P1_SIZE_META1];
} __attribute__((packed));

/**
 * Mediatek P1 driver output video device meta buffer: META2
 */
struct mtk_p1_metabuf_meta2 {
  uint8_t data[MTK_P1_SIZE_META2];
} __attribute__((packed));

/* -------------------------------------------------------------------------- */
/* init functions                                                             */
/* -------------------------------------------------------------------------- */
inline void mtk_p1_init_metabuf_tuning(struct mtk_p1_metabuf_tuning* s) {
  memset(reinterpret_cast<void*>(s), 0x0, sizeof(struct mtk_p1_metabuf_tuning));
}

inline void mtk_p1_init_metabuf_meta1(struct mtk_p1_metabuf_meta1* s) {
  memset(reinterpret_cast<void*>(s), 0x0, sizeof(struct mtk_p1_metabuf_meta1));
}

inline void mtk_p1_init_metabuf_meta2(struct mtk_p1_metabuf_meta2* s) {
  memset(reinterpret_cast<void*>(s), 0x0, MTK_P1_SIZE_META2);
}

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_MTK_P1_METABUF_H_
