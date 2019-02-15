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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ISPIO_PORT_INDEX_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ISPIO_PORT_INDEX_H_

/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 *
 ******************************************************************************/
namespace NSImageio {
namespace NSIspio {
enum EPortIndex {
  EPortIndex_TG1I,  // 0
  EPortIndex_TG2I,
  EPortIndex_CAMSV_TG1I,
  EPortIndex_CAMSV_TG2I,
  EPortIndex_LSCI,
  EPortIndex_CQI,  // 5
  EPortIndex_IMGO,
  EPortIndex_UFEO,
  EPortIndex_RRZO,
  EPortIndex_CAMSV_IMGO,
  EPortIndex_CAMSV2_IMGO,  // 10
  EPortIndex_LCSO,
  EPortIndex_AAO,
  EPortIndex_AFO,
  EPortIndex_PDO,
  EPortIndex_EISO,  // 15
  EPortIndex_FLKO,
  EPortIndex_RSSO,
  EPortIndex_PSO,
  EPortIndex_IMGI,
  EPortIndex_IMGBI,  // 20
  EPortIndex_IMGCI,
  EPortIndex_VIPI,
  EPortIndex_VIP2I,
  EPortIndex_VIP3I,
  EPortIndex_UFDI,  // 25
  EPortIndex_LCEI,
  EPortIndex_DMGI,
  EPortIndex_DEPI,
  EPortIndex_TDRI,
  EPortIndex_IMG2O,
  EPortIndex_IMG2BO,
  EPortIndex_IMG3O,
  EPortIndex_IMG3BO,
  EPortIndex_IMG3CO,
  EPortIndex_MFBO,
  EPortIndex_FEO,
  EPortIndex_WROTO,
  EPortIndex_WDMAO,
  EPortIndex_JPEGO,
  EPortIndex_VENC_STREAMO,
  EPortIndex_RAWI,
  EPortIndex_CAMSV_0_TGI,
  EPortIndex_CAMSV_1_TGI,
  EPortIndex_CAMSV_2_TGI,
  EPortIndex_CAMSV_3_TGI,
  EPortIndex_CAMSV_4_TGI,
  EPortIndex_CAMSV_5_TGI,
  EPortIndex_REGI,
  EPortIndex_WPEO,
  EPortIndex_WPEI,
  EPortIndex_MSKO,
  EPortIndex_PAK2O,
  EPortIndex_TUNING,
  EPortIndex_META1,
  EPortIndex_META2,
  EPortIndex_UNKNOWN  // always put at the bottom of this enum
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSIspio
};      // namespace NSImageio
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ISPIO_PORT_INDEX_H_
