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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1CONFIG_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1CONFIG_H_

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_SUPPORT_3A
#define P1NODE_SUPPORT_3A (1)
#undef P1NODE_SUPPORT_ISP
#define P1NODE_SUPPORT_ISP (1)
#undef P1NODE_SUPPORT_PERFRAME_CTRL
#define P1NODE_SUPPORT_PERFRAME_CTRL (0)

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_SUPPORT_LCS
#define P1NODE_SUPPORT_LCS (1)  // cros
#undef P1NODE_SUPPORT_RSS
#define P1NODE_SUPPORT_RSS (0)
#undef P1NODE_SUPPORT_FSC
#define P1NODE_SUPPORT_FSC (0)

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_SUPPORT_RRZ_DST_CTRL
#define P1NODE_SUPPORT_RRZ_DST_CTRL (1)
//
#undef P1NODE_SUPPORT_CONFIRM_BUF_PA
#define P1NODE_SUPPORT_CONFIRM_BUF_PA (1)
#undef P1NODE_SUPPORT_CONFIRM_BUF_PA_VA
#define P1NODE_SUPPORT_CONFIRM_BUF_PA_VA (P1NODE_SUPPORT_CONFIRM_BUF_PA && (0))

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_SUPPORT_BUFFER_TUNING_DUMP
#if (MTKCAM_HW_NODE_USING_TUNING_UTILS > 0)
#define P1NODE_SUPPORT_BUFFER_TUNING_DUMP (1)
#else
#define P1NODE_SUPPORT_BUFFER_TUNING_DUMP (0)
#endif

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_USING_MTK_LDVT
#ifdef USING_MTK_LDVT
#define P1NODE_USING_MTK_LDVT (1)
#else
#define P1NODE_USING_MTK_LDVT (0)
#endif

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_USING_CTRL_3A_LIST
#if (MTKCAM_HW_NODE_USING_3A_LIST > 0)
#define P1NODE_USING_CTRL_3A_LIST (1)
#else
#define P1NODE_USING_CTRL_3A_LIST (0)
#endif
#define P1NODE_USING_CTRL_3A_LIST_PREVIOUS \
  (P1NODE_USING_CTRL_3A_LIST && (1))  // USING_PREVIOUS_3A_LIST

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_USING_DRV_SET_RRZ_CBFP_EXP_SKIP
#define P1NODE_USING_DRV_SET_RRZ_CBFP_EXP_SKIP (1)
#undef P1NODE_USING_DRV_QUERY_CAPABILITY_EXP_SKIP
#define P1NODE_USING_DRV_QUERY_CAPABILITY_EXP_SKIP (1)

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_USING_DRV_IO_PIPE_EVENT
#define P1NODE_USING_DRV_IO_PIPE_EVENT (0)

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_ENABLE_CHECK_CONFIG_COMMON_PORPERTY
#define P1NODE_ENABLE_CHECK_CONFIG_COMMON_PORPERTY (0)

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_HAVE_AEE_FEATURE
#if (HWNODE_HAVE_AEE_FEATURE > 0)
#define P1NODE_HAVE_AEE_FEATURE (1)
#else
#define P1NODE_HAVE_AEE_FEATURE (0)
#endif

/******************************************************************************
 *
 ******************************************************************************/
#undef P1NODE_BUILD_LOG_LEVEL_DEFAULT
#if (MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT > 3)
#define P1NODE_BUILD_LOG_LEVEL_DEFAULT (4)  // for ENG build
#elif MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT > 2
#define P1NODE_BUILD_LOG_LEVEL_DEFAULT (3)  // for USERDEBUG build
#else
#define P1NODE_BUILD_LOG_LEVEL_DEFAULT (2)  // for USER build
#endif

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1CONFIG_H_
