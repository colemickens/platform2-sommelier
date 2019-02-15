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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_ISTREAMINGFEATUREPIPE_VAR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_ISTREAMINGFEATUREPIPE_VAR_H_

#define VAR_APP_MODE "common.app.mode"
#define VAR_P1_TS "common.p1.ts"
#define VAR_DEBUG_DUMP "common.debug.dump"
#define VAR_DEBUG_DUMP_HINT "common.debug.dump.hint"

#define VAR_PREV_HOLDER "prev_holder"
#define VAR_CURR_HOLDER "curr_holder"
#define VAR_PREV_RSSO "prev_rsso"
#define VAR_CURR_RSSO "curr_rsso"
#define VAR_FD_CROP_ACTIVE_REGION "active.fd_crop"

#define VAR_EIS_FACTOR "eis.factor"
#define VAR_EIS_LMV_DATA "eis.lmv.data"
// IMGO2IMGI ( Hal1 Use)
#define VAR_IMGO_2IMGI_ENABLE "imgo.2imgi.enabled"
#define VAR_IMGO_2IMGI_P1CROP "imgo.2imgi.p1_scalar_crop"

#define VAR_P1RAW_TWIN_STATUS "p1raw.twin.status"

// for 3DNR LMV
#define VAR_LMV_SWITCH_OUT_RST "lmv.SwitchOutResult"
#define VAR_LMV_VALIDITY "lmv.Validity"
// for 3DNR
#define VAR_3DNR_ISO_THRESHOLD "3dnr.isoThreshold"
#define VAR_3DNR_GYRO "3dnr.gyro"
#define VAR_3DNR_ISO "3dnr.iso"
#define VAR_3DNR_MV_INFO "3dnr.mvinfo"
#define VAR_3DNR_EIS_IS_CRZ_MODE "3dnr.eis.isCRZMode"
#define VAR_3DNR_CAN_ENABLE_ON_FRAME "3dnr.canEnable3dnr"

// for Hal1 Use
#define VAR_HAL1_HAL_IN_METADATA "hal1.hal_in_meta"
#define VAR_HAL1_APP_IN_METADATA "hal1.app_in_meta"
#define VAR_HAL1_HAL_OUT_METADATA "hal1.hal_out_meta"
#define VAR_HAL1_APP_OUT_METADATA "hal1.app_out_meta"
#define VAR_HAL1_P1_OUT_RRZ_SIZE "hal1.p1_out_rrz_size"

// for Dynamic Tuning
#define VAR_TUNING_HAL_IN_METADATA "tuning.hal_in_meta"
#define VAR_TUNING_APP_IN_METADATA "tuning.app_in_meta"
#define VAR_TUNING_HAL_OUT_METADATA "tuning.hal_out_meta"
#define VAR_TUNING_APP_OUT_METADATA "tuning.app_out_meta"
#define VAR_TUNING_IIMAGEBUF_LCSO "tuning.iimageBuf.lcso"

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_ISTREAMINGFEATUREPIPE_VAR_H_
