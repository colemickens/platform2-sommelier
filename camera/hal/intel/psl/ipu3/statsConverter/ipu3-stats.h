/*
 * Copyright (C) 2017-2018 Intel Corporation.
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

#ifndef __IPU3_STATS_H
#define __IPU3_STATS_H

#include <linux/intel-ipu3.h>
#include "stats_3a_public.h"

#include "ia_aiq_types.h"

#define AWB_FR_MAX_GRID_WIDTH   32
#define AWB_FR_MAX_GRID_HEIGHT  24
#define AF_MAX_GRID_HEIGHT  24
#define AF_MAX_GRID_WIDTH   32
#define AWB_PUBLIC_NUM_OF_SETS_IN_BUFFER (60 + 20)
#define AWB_PUBLIC_NUM_OF_ITEMS_IN_SET 160
#define AE_NUM_OF_HIST_BINS 256
#define AWB_FR_PUBLIC_NUM_OF_ITEMS_IN_SET  (AWB_FR_MAX_GRID_WIDTH + 20)
#define AWB_FR_PUBLIC_NUM_OF_SETS_IN_BUFFER AWB_FR_MAX_GRID_HEIGHT
#define AWB_FR_BUFF_RATIO 2 /**< AWB_FR stats buffer ratio */
#define AF_PUBLIC_NUM_OF_SETS_IN_BUFFER AF_MAX_GRID_HEIGHT
#define AF_PUBLIC_NUM_OF_ITEMS_IN_SET   (AF_MAX_GRID_WIDTH + 20)
#define AF_BUFF_RATIO 2 /**< AF stats buffer ratio */

struct ipu3_stats_all_stats {
    struct ia_css_4a_statistics ia_css_4a_statistics;
    struct stats_4a_public_raw_buffer stats_4a_public_raw_buffer;
    struct ia_css_2500_4a_config ia_css_2500_4a_config;
};

void ipu3_stats_init_3a(struct ipu3_stats_all_stats *all_stats);
void ipu3_stats_get_3a(struct ipu3_stats_all_stats *all_stats,
                       const struct ipu3_uapi_stats_3a *isp_stats);
ia_err intel_skycam_statistics_convert(
    const ia_css_4a_statistics& statistics,
    ia_aiq_rgbs_grid* out_rgbs_grid,
    ia_aiq_af_grid* out_af_grid);

#endif
