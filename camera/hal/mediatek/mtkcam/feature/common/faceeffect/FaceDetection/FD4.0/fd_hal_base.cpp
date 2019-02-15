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
#define LOG_TAG "fd_hal_base"

#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <camera/hal/mediatek/mtkcam/feature/common/faceeffect/FaceDetection/FD4.0/fdvt_hal.h>
#include <mtkcam/feature/FaceDetection/fd_hal_base.h>

/*******************************************************************************
 *
 *****************************************************************************/
std::shared_ptr<halFDBase> halFDBase::createInstance(HalFDObject_e eobject,
                                                     int openId) {
  if (eobject == HAL_FD_OBJ_SW) {
    return std::shared_ptr<halFDBase>(halFDVT::getInstance(openId));
  }
  if (eobject == HAL_FD_OBJ_HW) {
    return std::shared_ptr<halFDBase>(halFDVT::getInstance(openId));
  }
  if (eobject == HAL_FD_OBJ_FDFT_SW) {
    return std::shared_ptr<halFDBase>(halFDVT::getInstance(openId));
  }
  return nullptr;
}
