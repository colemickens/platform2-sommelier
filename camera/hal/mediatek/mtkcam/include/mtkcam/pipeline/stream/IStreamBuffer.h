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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_ISTREAMBUFFER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_ISTREAMBUFFER_H_
//
#include <list>
#include <memory>
#include <string>
#include <vector>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include "IUsersManager.h"
#include "IStreamInfo.h"
#include <mtkcam/utils/imgbuf/IGbmImageBufferHeap.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/**
 * Camera stream buffer status.
 */
struct STREAM_BUFFER_STATUS {
  typedef STREAM_BUFFER_STATUS T;
  enum {
    ERROR = (1U << 0), /*!< The buffer may contain invalid data. */
    WRITE = (1U << 1), /*!< The buffer's content has been touched. */
    WRITE_OK = T::WRITE,
    WRITE_ERROR = T::WRITE | T::ERROR,
  };
};

/**
 * An interface of stream buffer.
 */
class IStreamBuffer : public virtual IUsersManager {
 public:  ////                            Attributes.
  virtual char const* getName() const = 0;
  virtual MUINT32 getStatus() const = 0;
  virtual MBOOL hasStatus(MUINT32 mask) const = 0;
  virtual MVOID markStatus(MUINT32 mask) = 0;
  virtual MVOID clearStatus() = 0;

  /**
   * dump to a string for debug.
   */
  virtual std::string toString() const = 0;
};

/**
 * An interface template of stream buffer.
 *
 * @param <_IStreamInfoT_> the type of stream info interface.
 *
 * @param <_IBufferT_> the type of buffer interface.
 */
template <class _IStreamInfoT_, class _IBufferT_>
class TIStreamBuffer : public virtual IStreamBuffer {
 public:  ////                            Definitions.
  typedef _IStreamInfoT_ IStreamInfoT;
  typedef _IBufferT_ IBufferT;

 public:  ////                            Attributes.
  virtual std::shared_ptr<IStreamInfoT> getStreamInfo() const = 0;

 public:  ////                            Operations.
  /**
   * Release the buffer and unlock its use.
   *
   * @remark Make sure that the caller name must be the same as that passed
   *  during tryReadLock or tryWriteLock.
   */
  virtual MVOID unlock(char const* szCallName, IBufferT* pBuffer) = 0;

  /**
   * A reader must try to lock the buffer for reading.
   *
   * @remark The same caller name must be passed to unlock.
   */
  virtual IBufferT* tryReadLock(char const* szCallName) = 0;

  /**
   * A writer must try to lock the buffer for writing.
   *
   * @remark The same caller name must be passed to unlock.
   */
  virtual IBufferT* tryWriteLock(char const* szCallName) = 0;
};

/**
 * An interface of metadata stream buffer.
 */
class IMetaStreamBuffer : public TIStreamBuffer<IMetaStreamInfo, IMetadata> {
 public:  ////                            Definitions.
 public:  ////                            Attributes.
  /**
   * MTRUE indicates that the meta settnigs are identical to the most-recently
   * submitted meta settnigs; otherwise MFALSE.
   */
  virtual MBOOL isRepeating() const = 0;

 public:  ////                            Operations.
};

/**
 * An interface of image stream buffer.
 */
class IImageStreamBuffer
    : public TIStreamBuffer<IImageStreamInfo, IImageBufferHeap> {
 public:  ////                            Definitions.
 public:  ////                            Attributes.
 public:  ////                            Operations.
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_ISTREAMBUFFER_H_
