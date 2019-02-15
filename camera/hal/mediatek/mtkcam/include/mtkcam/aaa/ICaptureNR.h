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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_ICAPTURENR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_ICAPTURENR_H_

#include <camera_custom_capture_nr.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/module/module.h>

/******************************************************************************
 *
 ******************************************************************************/
class ISwNR {
 public:
  struct SWNRParam {
    MUINT32 iso;
    MBOOL isMfll;
    MINT32 perfLevel;
    //
    SWNRParam() : iso(0), isMfll(MFALSE), perfLevel(eSWNRPerf_Default) {}
  };

 public:
  virtual ~ISwNR() {}

  static ISwNR* createInstance(MUINT32 const openId);

 public:
  virtual MBOOL doSwNR(SWNRParam const& param, NSCam::IImageBuffer* pBuf) = 0;
  virtual MBOOL doSwNR(SWNRParam const& param,
                       NSCam::IImageBuffer* pBuf,
                       MINT32 magicNo) {
    return MTRUE;
  }
  // [in/out] halMetadata
  virtual MBOOL getDebugInfo(NSCam::IMetadata* halMetadata) = 0;
};

/**
 * @brief The definition of the maker of ISwNR instance.
 */
typedef ISwNR* (*SwNR_FACTORY_T)(MUINT32 const openId);
#define MAKE_SwNR(...) \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_SW_NR, SwNR_FACTORY_T, __VA_ARGS__)

#define MAKE_SWNR_IPC(...)                                           \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_SW_NR_IPC, SwNR_FACTORY_T, \
                     __VA_ARGS__)

/******************************************************************************
 *
 ******************************************************************************/
#endif  //  CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_ICAPTURENR_H_
