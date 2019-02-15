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

#define LOG_TAG "MtkCam/IBUS"

#include <mtkcam/feature/utils/ImageBufferUtils.h>
#include "base/strings/stringprintf.h"
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/def/UITypes.h>
#include <mtkcam/def/ImageFormat.h>
#include <mtkcam/utils/std/Format.h>
#include <mtkcam/utils/imgbuf/ImageBufferHeap.h>
#include <mtkcam/utils/imgbuf/IGbmImageBufferHeap.h>
#include <memory>
#include <mutex>
#include <string>

#define BUFFER_USAGE_SW                                        \
  eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN | \
      eBUFFER_USAGE_HW_CAMERA_READWRITE

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(ImageBufferUtils);

MBOOL ImageBufferUtils::allocBuffer(std::shared_ptr<IImageBuffer>* imageBuf,
                                    MUINT32 w,
                                    MUINT32 h,
                                    MUINT32 format,
                                    MBOOL isContinuous) {
  MBOOL ret = MTRUE;

  std::shared_ptr<IImageBuffer> pBuf;

#if 1  // check later, format.cpp has been removed
  size_t planeCount = 3;
#else
  size_t planeCount = queryPlaneCount(format);
#endif
  MINT32 bufBoundaryInBytes[3] = {0};
  MUINT32 strideInBytes[3] = {0};
#if 0  // check later
    for (size_t i = 0; i < planeCount; i++) {
        strideInBytes[i] =
            (queryPlaneWidthInPixels(format, i, w) *
             queryPlaneBitsPerPixel(format, i) + 7) / 8;

        CAM_LOGV("allocBuffer strideInBytes[%zu] strideInBytes(%d)",
                 i, strideInBytes[i]);
    }
#endif
  if (isContinuous) {
    // to avoid non-continuous multi-plane memory,
    // allocate memory in blob format and map it to ImageBuffer
    MUINT32 allPlaneSize = 0;
#if 0  // check later
        for (size_t i = 0; i < planeCount; i++) {
            allPlaneSize +=
                ((queryPlaneWidthInPixels(format, i, w) *
                  queryPlaneBitsPerPixel(format, i) + 7) / 8) *
                queryPlaneHeightInPixels(format, i, h);
        }
#endif
    CAM_LOGV("allocBuffer all plane size(%d)", allPlaneSize);

    // allocate blob buffer
    IImageBufferAllocator::ImgParam blobParam(allPlaneSize,
                                              bufBoundaryInBytes[0]);

    IImageBufferAllocator* allocator = IImageBufferAllocator::getInstance();
    std::shared_ptr<IImageBuffer> tmpImageBuffer =
        allocator->alloc(LOG_TAG, blobParam);
    if (tmpImageBuffer == NULL) {
      CAM_LOGE("tmpImageBuffer is NULL");
      return MFALSE;
    }

    // NOTE: after sp holds the allocated buffer, free can be called anywhere
    allocator->free(tmpImageBuffer);

    if (!tmpImageBuffer->lockBuf(LOG_TAG, BUFFER_USAGE_SW)) {
      CAM_LOGE("lock Buffer failed");
      return MFALSE;
    }

    // encapsulate tmpImageBuffer into external ImageBuffer
    IImageBufferAllocator::ImgParam extParam(format, MSize(w, h), strideInBytes,
                                             bufBoundaryInBytes, planeCount);
    PortBufInfo_v1 portBufInfo = PortBufInfo_v1(
        tmpImageBuffer->getFD(), tmpImageBuffer->getBufVA(0), 0, 0, 0);

    std::shared_ptr<ImageBufferHeap> heap =
        ImageBufferHeap::create(LOG_TAG, extParam, portBufInfo);

    if (heap == NULL) {
      CAM_LOGE("pHeap is NULL");
      return MFALSE;
    }

    pBuf = heap->createImageBuffer();

    // add into list for management
    {
      std::lock_guard<std::mutex> _l(mInternalBufferListLock);
      mInternalBufferList.emplace(reinterpret_cast<MINTPTR>(pBuf.get()),
                                  tmpImageBuffer);
    }
  } else {
#if 1  // check later
    IImageBufferAllocator::ImgParam imgParam(MSize(w, h), (*strideInBytes),
                                             (*bufBoundaryInBytes));
    imgParam.imgFormat = format;
    std::shared_ptr<IImageBufferHeap> heap = IGbmImageBufferHeap::create(
        LOG_TAG, imgParam,
        // check_later
        IGbmImageBufferHeap::AllocExtraParam(eBUFFER_USAGE_HW_TEXTURE), MFALSE);

#else
    IIonImageBufferHeap::AllocImgParam_t imgParam(
        format, MSize(w, h), strideInBytes, bufBoundaryInBytes, planeCount);

    std::shared_ptr<IIonImageBufferHeap> heap = IIonImageBufferHeap::create(
        LOG_TAG, imgParam, IIonImageBufferHeap::AllocExtraParam(), MFALSE);
#endif

    if (heap == NULL) {
      CAM_LOGE("heap is NULL");
      return MFALSE;
    }

    pBuf = heap->createImageBuffer();
  }

  if (pBuf == NULL) {
    CAM_LOGE("pBuf is NULL");
    return MFALSE;
  }

  if (!pBuf->lockBuf(LOG_TAG, BUFFER_USAGE_SW)) {
    CAM_LOGE("lock Buffer failed");
    ret = MFALSE;
  } else {
    // flush
    CAM_LOGD("allocBuffer addr(%p) size(%dx%d) format(0x%x)", pBuf.get(), w, h,
             format);

    {
      std::string msg("allocBuffer");
      for (size_t i = 0; i < planeCount; i++) {
        base::StringAppendF(&msg, " plane:va(%zu:%#" PRIxPTR ")", i,
                            pBuf->getBufVA(i));
      }
      CAM_LOGD("%s", msg.c_str());
    }
    *imageBuf = pBuf;
  }

  return ret;
}

void ImageBufferUtils::deallocBuffer(IImageBuffer* pBuf) {
  if (pBuf == nullptr) {
    CAM_LOGD("pBuf is NULL, do nothing");
    return;
  }

  // unlock image buffer
  pBuf->unlockBuf(LOG_TAG);

  // unlock internal buffer
  {
    std::lock_guard<std::mutex> _l(mInternalBufferListLock);
    const MINTPTR key = reinterpret_cast<MINTPTR>(pBuf);
    auto search = mInternalBufferList.find(key);
    if (search != mInternalBufferList.end()) {
      std::shared_ptr<IImageBuffer> tmpImageBuffer = search->second;
      if (tmpImageBuffer.get()) {
        tmpImageBuffer->unlockBuf(LOG_TAG);
        mInternalBufferList.erase(key);
      }
    }
  }
}

void ImageBufferUtils::deallocBuffer(std::shared_ptr<IImageBuffer>* pBuf) {
  deallocBuffer(pBuf->get());
}

std::shared_ptr<IImageBuffer> ImageBufferUtils::createBufferAlias(
    std::shared_ptr<IImageBuffer> pOriginalBuf,
    MUINT32 w,
    MUINT32 h,
    EImageFormat format) {
  MBOOL ret = MTRUE;

  MSize imgSize(w, h);
  size_t bufStridesInBytes[3] = {0};

  std::shared_ptr<IImageBuffer> pBuf = nullptr;
  std::shared_ptr<IImageBufferHeap> pBufHeap = nullptr;

  if (!pOriginalBuf) {
    CAM_LOGE("pOriginalBuf is null");
    ret = MFALSE;
    goto lbExit;
  }

  CAM_LOGD("pOriginalBuf is %p", pOriginalBuf.get());

  pBufHeap = pOriginalBuf->getImageBufferHeap();
  if (!pBufHeap) {
    CAM_LOGE("pBufHeap is null");
    ret = MFALSE;
    goto lbExit;
  }

  if (pBufHeap->getImgFormat() != eImgFmt_BLOB) {
    CAM_LOGE("heap buffer type must be BLOB=0x%x, this is 0x%x", eImgFmt_BLOB,
             pBufHeap->getImgFormat());
    ret = MFALSE;
    goto lbExit;
  }

#if 1
  switch (format) {
    case eImgFmt_Y8:
    case eImgFmt_JPEG:
      bufStridesInBytes[0] = w;
      break;
    case eImgFmt_I420:
      bufStridesInBytes[0] = w;
      bufStridesInBytes[1] = w / 2;
      bufStridesInBytes[2] = w / 2;
      break;
    case eImgFmt_YUY2:
      bufStridesInBytes[0] = w * 2;
      bufStridesInBytes[1] = w;
      bufStridesInBytes[2] = w;
      break;
    default:
      CAM_LOGE("unsupported format(0x%x)", format);
      ret = MFALSE;
      goto lbExit;
  }
#else
  GetStride(w, format, bufStridesInBytes);
#endif

  // create new buffer
  pBuf = pBufHeap->createImageBuffer_FromBlobHeap((size_t)0, format, imgSize,
                                                  bufStridesInBytes);

  if (!pBuf) {
    CAM_LOGE("can't makeBufferAlias size(%dx%d) format(0x%x)", w, h, format);
    ret = MFALSE;
    goto lbExit;
  }

  // unlock old buffer
  if (!pOriginalBuf->unlockBuf(LOG_TAG)) {
    CAM_LOGE("can't unlock pOriginalBuf");
  }

  // lock new buffer
  if (!pBuf->lockBuf(LOG_TAG, BUFFER_USAGE_SW)) {
    CAM_LOGE("can't lock pBuf");
    ret = MFALSE;
    goto lbExit;
  }

lbExit:
  return pBuf;
}

MBOOL ImageBufferUtils::removeBufferAlias(
    std::shared_ptr<IImageBuffer> pOriginalBuf,
    std::shared_ptr<IImageBuffer> pAliasBuf) {
  MBOOL ret = MTRUE;

  if ((pOriginalBuf == NULL) || (pAliasBuf == NULL)) {
    CAM_LOGE("ImageBuffer is NULL: pOriginalBuf(%p) pAliasBuf(%p)",
             pOriginalBuf.get(), pAliasBuf.get());
    ret = MFALSE;
    goto lbExit;
  }

  // destroy alias
  if (!pAliasBuf->unlockBuf(LOG_TAG)) {
    CAM_LOGE("can't unlock pAliasBuf");
    ret = MFALSE;
    goto lbExit;
  }

  // re-lock the original buffer
  if (!pOriginalBuf->lockBuf(LOG_TAG, BUFFER_USAGE_SW)) {
    CAM_LOGE("can't lock pOriginalBuf");
    ret = MFALSE;
    goto lbExit;
  }

lbExit:
  return ret;
}
#if 1  // check later
MBOOL ImageBufferUtils::createBuffer(std::shared_ptr<IImageBuffer>* output_buf,
                                     std::shared_ptr<IImageBuffer>* input_buf) {
  MINT32 planeCount = input_buf->getPlaneCount();

  MINT32 format = eImgFmt_UNKNOWN;
  if (planeCount == 3) {
    format = *(reinterpret_cast<MINT32*>(input_buf->getBufVA(2)));
  } else {
    format = input_buf->getImgFormat();
  }

  // create buffer
  MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
  MUINT32 bufStridesInBytes[3] = {0, 0, 0};

  for (int i = 0; i < planeCount; ++i) {
    bufStridesInBytes[i] = input_buf->getBufStridesInBytes(i);
  }

  IImageBufferAllocator::ImgParam imgParam = IImageBufferAllocator::ImgParam(
      (EImageFormat)format, input_buf->getImgSize(), bufStridesInBytes,
      bufBoundaryInBytes, (size_t)planeCount);
#if 1  // check later
  std::shared_ptr<IImageBufferHeap> pHeap = IGbmImageBufferHeap::create(
      LOG_TAG, imgParam,
      // check_later
      IGbmImageBufferHeap::AllocExtraParam(eBUFFER_USAGE_HW_TEXTURE), MFALSE);
  if (pHeap == NULL) {
    CAM_LOGE("[%s] Stuff ImageBufferHeap create fail", LOG_TAG);
    return MFALSE;
  }

  std::shared_ptr<IImageBuffer> pImgBuf = pHeap->createImageBuffer();
#else
  std::shared_ptr<IIonImageBufferHeap> pHeap =
      IIonImageBufferHeap::create(LOG_TAG, imgParam);
  if (pHeap == nullptr) {
    CAM_LOGE("[%s] Stuff ImageBufferHeap create fail", LOG_TAG);
    return MFALSE;
  }

  MINT imgFormat = pHeap->getImgFormat();
  ImgBufCreator creator(imgFormat);
  sp<IImageBuffer> pImgBuf = pHeap->createImageBuffer(&creator);
  if (pImgBuf == NULL) {
    CAM_LOGE("[%s] Stuff ImageBuffer create fail", LOG_TAG);
    return MFALSE;
  }
#endif

  // lock buffer
  if (!(pImgBuf->lockBuf(LOG_TAG, BUFFER_USAGE_SW))) {
    CAM_LOGE("[%s] Stuff ImageBuffer lock fail", LOG_TAG);
    return MFALSE;
  }

  *output_buf = pImgBuf;
  return MTRUE;
}
#endif
