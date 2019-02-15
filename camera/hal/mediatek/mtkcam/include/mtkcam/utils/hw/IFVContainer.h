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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_IFVCONTAINER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_IFVCONTAINER_H_

#include <cstdint>  // int32_t, int64_t, intptr_t
#include <memory>
#include <vector>  // std::vector

#include <mtkcam/def/common.h>  // MBOOL
#include <mtkcam/aaa/aaa_hal_common.h>

using std::vector;

#define FV_DATATYPE NS3Av3::AF_FRAME_INFO_T

namespace NSCam {
class VISIBILITY_PUBLIC IFVContainer {
  /* enums */
 public:
  enum eFVContainer_Opt {
    eFVContainer_Opt_Read = 0x1,
    eFVContainer_Opt_Write = 0x1 << 1,
    eFVContainer_Opt_RW = eFVContainer_Opt_Read | eFVContainer_Opt_Write,
  };

  /* interfaces */
 public:
  /**
   *  For eFVContainer_Opt_Read
   *  To get all avaliable focus values
   */
  virtual vector<FV_DATATYPE> query(void) = 0;

  /**
   *  For eFVContainer_Opt_Read
   *  To get the focus values in range [mg_start, mg_end]
   *  @param mg_start     magicNum from
   *  @param mg_end       magicNum until
   *
   *  magicNum is halMeta::MTK_P1NODE_PROCESSOR_MAGICNUM
   */
  virtual vector<FV_DATATYPE> query(const int32_t& mg_start,
                                    const int32_t& mg_end) = 0;

  /**
   *  For eFVContainer_Opt_Read
   *  To get the focus values in the giving set
   *  i-th return value = FV_DATA_ERROR if the timestamps vecMgs[i] is not found
   *  @param vecMgs       a set of magicNum
   *
   *  magicNum is halMeta::MTK_P1NODE_PROCESSOR_MAGICNUM
   */
  virtual vector<FV_DATATYPE> query(const vector<int32_t>& vecMgs) = 0;

  /**
   *  For eFVContainer_Opt_Write
   *  To push focus value into fv container and assign the key as input magicNum
   *  @param magicNum     the unique key for query focus values
   *  @param fv           the focus value
   *
   *  magicNum is halMeta::MTK_P1NODE_PROCESSOR_MAGICNUM
   */
  virtual MBOOL push(int32_t magicNum, FV_DATATYPE fv) = 0;

  /**
   *  To clear all focus values
   */
  virtual void clear(void) = 0;

  /**
   *  To dump all focus values
   */
  virtual void dumpInfo(void) = 0;

 public:
  static std::shared_ptr<IFVContainer> createInstance(char const* userId,
                                                      eFVContainer_Opt opt);

  ~IFVContainer() {}
};
}; /* namespace NSCam */

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_IFVCONTAINER_H_
