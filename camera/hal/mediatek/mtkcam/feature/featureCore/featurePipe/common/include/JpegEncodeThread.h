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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_JPEGENCODETHREAD_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_JPEGENCODETHREAD_H_

#include "ImageBufferPool.h"
#include <mtkcam/feature/featurePipe/SFPIO.h>
#include <mtkcam/utils/exif/IBaseCamExif.h>

#include <memory>
#include <queue>
#include <string>

class JpgEncHal;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class JpegEncodeThread {
 public:
  static std::shared_ptr<JpegEncodeThread> getInstance(
      const MSizeF& finalCrop, const char* filename = NULL);

  MBOOL compressJpeg(const std::shared_ptr<IIBuffer>& sourceBuffer,
                     bool markFrame = false);

 private:
  class WorkThread : public android::Thread {
    friend class JpegEncodeThread;

   public:
    explicit WorkThread(const std::shared_ptr<JpegEncodeThread>& outer)
        : wpEncoder(outer) {}

   private:
    bool threadLoop();
    std::queue<std::shared_ptr<IIBuffer> > mFullImgQueue;
    std::weak_ptr<JpegEncodeThread> wpEncoder;
    std::condition_variable mFullImgCond;
    std::mutex mFullImgLock;
  };

  explicit JpegEncodeThread(const MSizeF& finalCrop,
                            const char* filename = NULL);

  virtual ~JpegEncodeThread();

  bool init();

  bool prepareBuffers(MUINT32 width, MUINT32 height, EImageFormat format);

  bool makeExifHeader(MUINT32 width,
                      MUINT32 height,
                      MUINT8* exifBuf,
                      size_t* exifSize);

  bool encode(const std::shared_ptr<IIBuffer>& srcBuf);

  static std::mutex sSingletonLock;
  static android::wp<JpegEncodeThread> sEncoder;
  std::string mFilePath;
  std::shared_ptr<WorkThread> mpThread = nullptr;
  std::shared_ptr<IBufferPool> mpFullImgPool = nullptr;
  std::shared_ptr<IImageBuffer> mpJpegBuf = nullptr;
  IImageBufferAllocator* mpAllocator = NULL;
  JpgEncHal mJpgHal;
  MUINT8 mExifBuf[DBG_EXIF_SIZE] = {0};
  FILE* mpJpegFp = NULL;
  MSizeF mFinalCrop;
  size_t mExifSize = 0;
  bool mCropInfoSet = false;
  MUINT32 mFrameNum = 0;
};
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_JPEGENCODETHREAD_H_
