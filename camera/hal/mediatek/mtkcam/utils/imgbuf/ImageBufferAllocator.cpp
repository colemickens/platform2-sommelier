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

#define LOG_TAG "MtkCam/ImgBufAllocator"
//
#include "MyUtils.h"
#include <memory>
#include <mtkcam/utils/imgbuf/IGbmImageBufferHeap.h>

using NSCam::IImageBuffer;
using NSCam::IImageBufferAllocator;
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
IImageBufferAllocator* getImageBufferAllocator() {
  static IImageBufferAllocator* tmp = new IImageBufferAllocator();
  return tmp;
}

}  // namespace NSCam

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IImageBuffer> IImageBufferAllocator::alloc(
    char const* szCallerName,
    ImgParam const& rImgParam,
    ExtraParam const& rExtraParam,
    MBOOL const enableLog) {
  std::shared_ptr<IImageBuffer> pImgBuf = nullptr;
  //
  pImgBuf = alloc_gbm(szCallerName, rImgParam, rExtraParam, enableLog);

  MY_LOGD("pImgBuf use count:%ld", pImgBuf.use_count());
  return pImgBuf;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IImageBuffer> IImageBufferAllocator::alloc_gbm(
    char const* szCallerName,
    ImgParam const& rImgParam,
    ExtraParam const& rExtraParam,
    MBOOL const enableLog) {
  std::shared_ptr<IImageBuffer> pImgBuf = nullptr;
  //
  IGbmImageBufferHeap::AllocImgParam_t imgParam = rImgParam;
  IGbmImageBufferHeap::AllocExtraParam extraParam(rExtraParam.usage,
                                                  rExtraParam.nocache);
  if (eImgFmt_JPEG == rImgParam.imgFormat) {
    if (0 == imgParam.bufSize) {
      CAM_LOGE("Err imgParam: bufSize should not be 0 for JPEG memory");
      return NULL;
    }
    imgParam.imgFormat = eImgFmt_BLOB;
  }
  //
  std::shared_ptr<IImageBufferHeap> pHeap = IGbmImageBufferHeap::create(
      szCallerName, imgParam, extraParam, enableLog);
  if (pHeap == nullptr) {
    CAM_LOGE("NULL Heap");
    return NULL;
  }
  //
  if (eImgFmt_JPEG == rImgParam.imgFormat) {
    pImgBuf = pHeap->createImageBuffer_FromBlobHeap(
        0, rImgParam.imgFormat, rImgParam.jpgSize, rImgParam.bufStridesInBytes);
  } else {
    pImgBuf = pHeap->createImageBuffer();
  }
  if (pImgBuf == nullptr) {
    CAM_LOGE("createImageBuffer fail");
    return NULL;
  }
  MY_LOGD("alloc_gbm sucess,use count:%ld, %p", pImgBuf.use_count(),
          pImgBuf.get());
  //
  return pImgBuf;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
IImageBufferAllocator::free(std::shared_ptr<IImageBuffer> pImageBuffer) {
  if (pImageBuffer != nullptr) {
    pImageBuffer = nullptr;
  } else {
    CAM_LOGE("pImageBuffer is NULL");
  }
}
