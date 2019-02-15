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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_CAM_DBG_CAM_PARAM_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_CAM_DBG_CAM_PARAM_H_

#include <stddef.h>  // offsetof

#include "../../../../../common/hal/inc/custom/debug_exif/dbg_exif_param.h"
#include <cam_exif_tag_chksum.h>
// #include <custom/debug_exif/cam/dbg_cam_common_param1.h>

// using namespace dbg_cam_common_param_1;
// using namespace dbg_cam_mf_param_9;
// using namespace dbg_cam_n3d_param_1;
// using namespace dbg_cam_n3d_param_3;
// using namespace dbg_cam_sensor_param_0;
// using namespace dbg_cam_reservea_param_3;
// using namespace dbg_cam_reserveb_param_0;
// using namespace dbg_cam_reservec_param_0;
//
#define DEBUF_CAM_TOT_MODULE_NUM 7  // should be modified
#define DEBUF_CAM_TAG_MODULE_NUM 6  // should be modified

#define MAXIMUM_CAM_DEBUG_COMM_SIZE 32

typedef union {
  struct {
    uint32_t chkSum;
    uint32_t ver;
  };
  uint8_t Data[MAXIMUM_CAM_DEBUG_COMM_SIZE];
} CAM_DEBUG_COMM_T;
static_assert(sizeof(CAM_DEBUG_COMM_T) == MAXIMUM_CAM_DEBUG_COMM_SIZE,
              "CAM_DEBUG_COM_T size mismatch");

typedef struct {
  uint32_t u4Size;
  CAM_DEBUG_COMM_T rCMN;
  CAM_DEBUG_COMM_T rMF;
  CAM_DEBUG_COMM_T rN3D;
  CAM_DEBUG_COMM_T rSENSOR;
  CAM_DEBUG_COMM_T rRESERVEA;
  CAM_DEBUG_COMM_T rRESERVEB;
} COMMON_DEBUG_INFO_T;

typedef struct DEBUG_CAM_INFO_S {
  struct Header {
    uint32_t u4KeyID;
    uint32_t u4ModuleCount;
    uint32_t u4DbgCMNInfoOffset;
    uint32_t u4DbgMFInfoOffset;
    uint32_t u4DbgN3DInfoOffset;
    uint32_t u4DbgSENSORInfoOffset;
    uint32_t u4DbgRESERVEAInfoOffset;
    uint32_t u4DbgRESERVEBInfoOffset;
    uint32_t u4DbgRESERVECInfoOffset;
    COMMON_DEBUG_INFO_T rCommDebugInfo;
  } hdr;

  dbg_cam_common_param_1::DEBUG_CMN_INFO_T rDbgCMNInfo;
  dbg_cam_mf_param_9::DEBUG_MF_INFO_T rDbgMFInfo;
  dbg_cam_n3d_param_3::DEBUG_N3D_INFO_T rDbgN3DInfo;
  dbg_cam_sensor_param_0::DEBUG_SENSOR_INFO_T rDbgSENSORInfo;
  dbg_cam_reservea_param_3::DEBUG_RESERVEA_INFO_T rDbgRESERVEAInfo;
  dbg_cam_reserveb_param_0::DEBUG_RESERVEB_INFO_T rDbgRESERVEBInfo;
  dbg_cam_reservec_param_0::DEBUG_RESERVEC_INFO_T rDbgRESERVECInfo;
} DEBUG_CAM_INFO_T;

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace Custom {
static const DEBUG_CAM_INFO_T::Header sDbgExifBufHeader = {
    .u4KeyID = DEBUG_EXIF_KEYID_CAM,
    .u4ModuleCount =
        DBGEXIF_MODULE_NUM(DEBUF_CAM_TOT_MODULE_NUM, DEBUF_CAM_TAG_MODULE_NUM),
    .u4DbgCMNInfoOffset = offsetof(DEBUG_CAM_INFO_T, rDbgCMNInfo),
    .u4DbgMFInfoOffset = offsetof(DEBUG_CAM_INFO_T, rDbgMFInfo),
    .u4DbgN3DInfoOffset = offsetof(DEBUG_CAM_INFO_T, rDbgN3DInfo),
    .u4DbgSENSORInfoOffset = offsetof(DEBUG_CAM_INFO_T, rDbgSENSORInfo),
    .u4DbgRESERVEAInfoOffset = offsetof(DEBUG_CAM_INFO_T, rDbgRESERVEAInfo),
    .u4DbgRESERVEBInfoOffset = offsetof(DEBUG_CAM_INFO_T, rDbgRESERVEBInfo),
    .u4DbgRESERVECInfoOffset = offsetof(DEBUG_CAM_INFO_T, rDbgRESERVECInfo),
    .rCommDebugInfo = {
        .rCMN = {.chkSum = CHKSUM_DBG_COMM_PARAM,
                 .ver = dbg_cam_common_param_1::CMN_DEBUG_TAG_VERSION_DP},
        .rMF = {.chkSum = CHKSUM_DBG_MF_PARAM,
                .ver = dbg_cam_mf_param_9::MF_DEBUG_TAG_VERSION_DP},
        .rN3D = {.chkSum = CHKSUM_DBG_N3D_PARAM,
                 .ver = dbg_cam_n3d_param_3::N3D_DEBUG_TAG_VERSION_DP},
        .rSENSOR = {.chkSum = CHKSUM_DBG_SENSOR_PARAM,
                    .ver = dbg_cam_sensor_param_0::SENSOR_DEBUG_TAG_VERSION_DP},
        .rRESERVEA =
            {.chkSum = CHKSUM_DBG_RESERVEA_PARAM,
             .ver = dbg_cam_reservea_param_3::RESERVEA_DEBUG_TAG_VERSION_DP},
        .rRESERVEB = {
            .chkSum = CHKSUM_DBG_RESERVEB_PARAM,
            .ver = dbg_cam_reserveb_param_0::RESERVEB_DEBUG_TAG_VERSION_DP}}};

static const debug_exif_buffer_info sDbgExifBufInfo_cam = {
    .header_size = sizeof(DEBUG_CAM_INFO_T::Header),
    .body_size = sizeof(DEBUG_CAM_INFO_T) - sizeof(DEBUG_CAM_INFO_T::Header),

    .header_context = &sDbgExifBufHeader,

    .body_layout =
        {
#define SET_MODULE_INFO(module_id, module_version, module_field)         \
  {                                                                      \
    module_id, {                                                         \
      module_id, module_version, sizeof(DEBUG_CAM_INFO_T::module_field), \
          offsetof(DEBUG_CAM_INFO_T, module_field)                       \
    }                                                                    \
  }

            SET_MODULE_INFO(DEBUG_EXIF_MID_CAM_CMN,
                            dbg_cam_common_param_1::CMN_DEBUG_TAG_VERSION,
                            rDbgCMNInfo),
            SET_MODULE_INFO(DEBUG_EXIF_MID_CAM_MF,
                            dbg_cam_mf_param_9::MF_DEBUG_TAG_VERSION,
                            rDbgMFInfo),
            SET_MODULE_INFO(DEBUG_EXIF_MID_CAM_N3D,
                            dbg_cam_n3d_param_3::N3D_DEBUG_TAG_VERSION,
                            rDbgN3DInfo),
            SET_MODULE_INFO(DEBUG_EXIF_MID_CAM_SENSOR,
                            dbg_cam_sensor_param_0::SENSOR_DEBUG_TAG_VERSION,
                            rDbgSENSORInfo),
            SET_MODULE_INFO(
                DEBUG_EXIF_MID_CAM_RESERVE1,
                dbg_cam_reservea_param_3::RESERVEA_DEBUG_TAG_VERSION,
                rDbgRESERVEAInfo),
            SET_MODULE_INFO(
                DEBUG_EXIF_MID_CAM_RESERVE2,
                dbg_cam_reserveb_param_0::RESERVEB_DEBUG_TAG_VERSION,
                rDbgRESERVEBInfo),
            SET_MODULE_INFO(
                DEBUG_EXIF_MID_CAM_RESERVE3,
                dbg_cam_reservec_param_0::RESERVEC_DEBUG_TAG_VERSION,
                rDbgRESERVECInfo),

#undef SET_MODULE_INFO
        },
};
};  // namespace Custom
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_CAM_DBG_CAM_PARAM_H_
