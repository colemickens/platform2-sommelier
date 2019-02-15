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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_LCS_LCS_TYPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_LCS_LCS_TYPE_H_

#include <mtkcam/def/common.h>

/*********************************************************************
 * Define
 *********************************************************************/

/*********************************************************************
 * ENUM
 *********************************************************************/

/**
 *@brief Return enum of LCS
 */
typedef enum {
  LCS_RETURN_NO_ERROR = 0,             //! The function work successfully
  LCS_RETURN_UNKNOWN_ERROR = 0x0001,   //! Unknown error
  LCS_RETURN_INVALID_DRIVER = 0x0002,  //! invalid driver object
  LCS_RETURN_API_FAIL = 0x0003,        //! api fail
  LCS_RETURN_INVALID_PARA = 0x0004,    //! invalid parameter
  LCS_RETURN_NULL_OBJ = 0x0005,        //! null object
  LCS_RETURN_MEMORY_ERROR = 0x0006     //! memory error
} LCS_ERROR_ENUM;

/**
 *@brief Return enum of LCS
 */
typedef enum { LCS_CAMERA_VER_1, LCS_CAMERA_VER_3 } LCS_CAMERA_VER_ENUM;

/*********************************************************************
 * Struct
 *********************************************************************/

/**
 * @brief LCS config structure
 *
 */
typedef struct LCS_HAL_CONFIG {
  LCS_CAMERA_VER_ENUM cameraVer;
  MUINT32 lcsOutWidth;
  MUINT32 lcsOutHeight;
  MUINT32 tgWidth;
  MUINT32 tgHeight;
} LCS_HAL_CONFIG_DATA;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_LCS_LCS_TYPE_H_
