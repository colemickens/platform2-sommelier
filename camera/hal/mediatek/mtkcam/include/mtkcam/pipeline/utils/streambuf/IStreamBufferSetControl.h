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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_ISTREAMBUFFERSETCONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_ISTREAMBUFFERSETCONTROL_H_
//
#include <mtkcam/pipeline/stream/IStreamBufferSet.h>
#include "StreamBuffers.h"
//
#include <memory>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/
class IStreamBufferSetControl : public IStreamBufferSet {
 public:  ////                Definitions.
  /**
   *
   */
  class IAppCallback {
   public:  ////            Operations.
    virtual MVOID updateFrame(MUINT32 const frameNo, MINTPTR const userId) = 0;
  };

  /**
   *
   */
  class IListener {
   public:
    /**
     * Invoked when the buffer set is updated.
     *
     * @param[in] pCookie: the listener's cookie.
     */
    virtual MVOID onStreamBufferSet_Updated(MVOID* pCookie) = 0;
  };

  /**
   *
   */
  template <class _StreamBuffer_>
  class IMap {
   public:  ////            Definitions.
    typedef _StreamBuffer_ StreamBufferT;

   public:  ////            Operations.
    virtual ssize_t add(std::shared_ptr<StreamBufferT> pBuffer) = 0;

    virtual ssize_t setCapacity(size_t size) = 0;

    virtual bool isEmpty() const = 0;

    virtual size_t size() const = 0;

    virtual ssize_t indexOfKey(StreamId_T const key) const = 0;

    virtual StreamId_T keyAt(size_t index) const = 0;

    virtual std::shared_ptr<StreamBufferT> const& valueAt(
        size_t index) const = 0;

    virtual std::shared_ptr<StreamBufferT> const& valueFor(
        StreamId_T const key) const = 0;
    virtual ~IMap() {}
  };

 public:  ////                Operations.
  virtual std::shared_ptr<IMap<IImageStreamBuffer> > editMap_AppImage() = 0;

  virtual std::shared_ptr<IMap<IMetaStreamBuffer> > editMap_AppMeta() = 0;

  virtual std::shared_ptr<IMap<HalImageStreamBuffer> > editMap_HalImage() = 0;

  virtual std::shared_ptr<IMap<HalMetaStreamBuffer> > editMap_HalMeta() = 0;

 public:  ////                Operations.
  static std::shared_ptr<IStreamBufferSetControl> create(
      MUINT32 frameNo, std::weak_ptr<IAppCallback> const& pAppCallback);

  virtual MERROR attachListener(std::weak_ptr<IListener> const& pListener,
                                MVOID* pCookie) = 0;

  virtual MUINT32 getFrameNo() const = 0;
  virtual ~IStreamBufferSetControl() {}
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_ISTREAMBUFFERSETCONTROL_H_
