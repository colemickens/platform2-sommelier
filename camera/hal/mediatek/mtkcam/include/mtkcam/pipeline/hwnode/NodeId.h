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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_NODEID_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_NODEID_H_
//

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 * HW Pipeline Node ID
 ******************************************************************************/
//  Notes:
//  Every node Id should belong to the global namespace, not to a specific
//  pipeline's namespace, so that we can easily reuse node's instances.
enum {
  eNODEID_UNKNOWN = 0x00,
  eNODEID_P1Node = 0x01,
  eNODEID_P1Node_main2 = 0x02,
  eNODEID_PDENode = 0x03,
  eNODEID_P2CaptureNode = 0x04,
  eNODEID_P2StreamNode = 0x05,
  eNODEID_P2Node = eNODEID_P2StreamNode,
  eNODEID_FDNode = 0x06,
  //
  eNODEID_JpegNode = 0x13,
  //
  eNODEID_RAW16 = 0x22,
  eNODEID_JpsNode = 0x34,
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_NODEID_H_
