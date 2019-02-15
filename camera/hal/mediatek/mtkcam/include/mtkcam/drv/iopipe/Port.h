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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_PORT_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_PORT_H_
//
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSIoPipe {

/******************************************************************************
 * @enum EPortType
 * @brief Pipe Port Type.
 ******************************************************************************/
enum EPortType {
  EPortType_Sensor, /*!< Sensor Port Type */
  EPortType_Memory, /*!< Memory Port Type */
};

/******************************************************************************
 * @enum EPortCapbility
 * @brief Pipe Port Capbility.
 ******************************************************************************/
enum EPortCapbility {
  EPortCapbility_None = 0x00, /*!< no specific */
  EPortCapbility_Cap = 0x01,  /*!< capture */
  EPortCapbility_Rcrd = 0x02, /*!< record */
  EPortCapbility_Disp = 0x03, /*!< display */
};

/******************************************************************************
 * @struct PortID
 * @brief Pipe Port ID (Descriptor).
 ******************************************************************************/
struct PortID {
 public:  ////    fields.
  /**
   * @var index
   *  port index
   */
  MUINT32 index : 8;

  /**
   * @var type
   * EPortType
   */
  EPortType type : 8;

  /**
   * @var inout
   * 0:in/1:out
   */
  MUINT32 inout : 1;

  /**
   * @var group
   * frame group(for burst queue support).
   */
  MUINT32 group : 4;
  /**
   * @var capbility
   * port capbility
   */
  EPortCapbility capbility : 2;
  //
  /**
   * @var reserved
   * reserved for future use.
   */
  MUINT32 reserved : 9;
  //

 public:  ////    Constructors.
  PortID(EPortType const _eType,
         MUINT32 const _index,
         MUINT32 const _inout,
         EPortCapbility const _capbility = EPortCapbility_None,
         MUINT32 const _group = 0) {
    type = _eType;
    index = _index;
    inout = _inout;
    capbility = _capbility;
    group = _group;
    reserved = 0;
  }
  //

  explicit PortID(MUINT32 const _value = 0) {
    *reinterpret_cast<MUINT32*>(this) = _value;
  }

 public:  ////    Operators.
  operator MUINT32() const { return *reinterpret_cast<MUINT32 const*>(this); }
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSIoPipe
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_PORT_H_
