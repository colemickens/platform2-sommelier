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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IHAL3ACB_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IHAL3ACB_H_

#include <mtkcam/def/common.h>

namespace NS3Av3 {
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class IHal3ACb {
 public:
  virtual ~IHal3ACb() {}

 public:
  virtual void doNotifyCb(MINT32 _msgType,
                          MINTPTR _ext1,
                          MINTPTR _ext2,
                          MINTPTR _ext3) = 0;

  enum ECb_T {
    eID_NOTIFY_3APROC_FINISH = 0,
    /*
        _ext1: magic number of current request
        _ext2: bit[0] OK/ERROR; bit[1] Init Ready
        _ext3: magic number of current used statistic magic number
    */

    eID_NOTIFY_READY2CAP = 1,

    eID_NOTIFY_CURR_RESULT = 2,
    /*
        _ext1: magic number of current result
        _ext2: metadata tag (key)
        _ext3: value
    */
    eID_NOTIFY_AE_RT_PARAMS = 3,
    /*
        _ext1: pointer of RT params.
        _ext2:
        _ext3:
    */
    eID_NOTIFY_VSYNC_DONE = 4,
    eID_NOTIFY_HDRD_RESULT = 5,
    eID_NOTIFY_LCS_ISP_PARAMS = 6,
    /*
        _ext1: magic number of current result
        _ext2: pointer of LCS params.
    */
    eID_MSGTYPE_NUM
  };
};

};  // namespace NS3Av3

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IHAL3ACB_H_
