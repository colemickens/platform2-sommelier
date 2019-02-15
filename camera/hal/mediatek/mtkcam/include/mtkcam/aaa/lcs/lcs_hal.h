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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_LCS_LCS_HAL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_LCS_LCS_HAL_H_

#include <mtkcam/aaa/lcs/lcs_type.h>
#include <mtkcam/utils/module/module.h>

/**
 *@brief LCS HAL class used by scenario
 */
class LcsHal {
 public:
  /**
   *@brief LcsHal constructor
   */
  LcsHal() = default;

  /**
   *@brief Create LcsHal object
   *@param[in] userName : user name,i.e. who create LcsHal object
   *@param[in] aSensorIdx : sensor index
   *@return
   *-LcsHal object
   */
  static LcsHal* CreateInstance(char const* userName,
                                const MUINT32& aSensorIdx);

  /**
   *@brief Destroy LcsHal object
   *@param[in] userName : user name,i.e. who destroy LcsHal object
   */
  virtual MVOID DestroyInstance(char const* userName) = 0;

  /**
   *@brief Initialization function
   *@return
   *-LCS_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 Init() = 0;

  /**
   *@brief Unitialization function
   *@return
   *-LCS_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 Uninit() = 0;

  /**
   *@brief Congif LCS
   *@param[in] aConfigData : config data
   *@return
   *-LCS_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 ConfigLcsHal(const LCS_HAL_CONFIG_DATA& aConfigData) = 0;

  /**
   *@brief LcsHal destructor
   */
  virtual ~LcsHal() = default;
};

/**
 * @brief The definition of the maker of LcsHal instance.
 */
typedef LcsHal* (*LcsHal_FACTORY_T)(char const* userName,
                                    const MUINT32& aSensorIdx);
#define MAKE_LcsHal(...)                                             \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_LCS_HAL, LcsHal_FACTORY_T, \
                     __VA_ARGS__)

#define MAKE_LCSHAL_IPC(...)                                             \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_LCS_HAL_IPC, LcsHal_FACTORY_T, \
                     __VA_ARGS__)

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_LCS_LCS_HAL_H_
