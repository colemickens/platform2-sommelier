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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_APP_APPSTREAMBUFFERS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_APP_APPSTREAMBUFFERS_H_
//

#include <memory>
#include <string>

#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <mtkcam/utils/imgbuf/IGraphicImageBufferHeap.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/******************************************************************************
 *
 ******************************************************************************/
/**
 * An implementation of app image stream buffer.
 */
class AppImageStreamBuffer
    : public Utils::TStreamBuffer<AppImageStreamBuffer, IImageStreamBuffer> {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Definitions.
  struct Allocator {
   public:  ////                            Data Members.
    std::shared_ptr<IStreamInfoT> mpStreamInfo;

   public:  ////                            Operations.
    explicit Allocator(std::shared_ptr<IStreamInfoT> pStreamInfo);

   public:  ////                            Operator.
    virtual std::shared_ptr<StreamBufferT> operator()(
        std::shared_ptr<IGraphicImageBufferHeap> pHeap,
        std::shared_ptr<IStreamInfoT> = nullptr);
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                                Data Members.
  std::shared_ptr<IGraphicImageBufferHeap> mImageBufferHeap;

 public:  ////                                Operations.
  AppImageStreamBuffer(
      std::shared_ptr<IStreamInfoT> pStreamInfo,
      std::shared_ptr<IGraphicImageBufferHeap> pImageBufferHeap,
      std::shared_ptr<IUsersManager> pUsersManager = nullptr);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Accessors.
  std::shared_ptr<IGraphicImageBufferHeap> getImageBufferHeap() const;
  virtual MINT getAcquireFence() const;
  virtual MVOID setAcquireFence(MINT fence);
  virtual MINT getReleaseFence() const;
  virtual MVOID setReleaseFence(MINT fence);

 public:  ////                                Attributes.
  virtual std::string toString() const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IUsersManager Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Operations.
  virtual ssize_t enqueUserGraph(std::shared_ptr<IUserGraph> pUserGraph);
};

/******************************************************************************
 *
 ******************************************************************************/
/**
 * An implementation of app metadata stream buffer.
 */
class AppMetaStreamBuffer
    : public Utils::TStreamBuffer<AppMetaStreamBuffer, IMetaStreamBuffer> {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Definitions.
  struct Allocator {
   public:  ////                            Data Members.
    std::shared_ptr<IStreamInfoT> mpStreamInfo;

   public:  ////                            Operations.
    explicit Allocator(std::shared_ptr<IStreamInfoT> pStreamInfo);

   public:  ////                            Operator.
    virtual std::shared_ptr<StreamBufferT> operator()();
    virtual std::shared_ptr<StreamBufferT> operator()(
        NSCam::IMetadata const& metadata);
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:                    ////                                Data Members.
  NSCam::IMetadata mMetadata;  // IBufferT-derived object.
  MBOOL mRepeating;            // Non-zero indicates repeating meta settings.

 public:  ////                                Operations.
  AppMetaStreamBuffer(std::shared_ptr<IStreamInfoT> pStreamInfo,
                      std::shared_ptr<IUsersManager> pUsersManager = nullptr);
  AppMetaStreamBuffer(std::shared_ptr<IStreamInfoT> pStreamInfo,
                      NSCam::IMetadata const& metadata,
                      std::shared_ptr<IUsersManager> pUsersManager = nullptr);

 public:  ////                                Attributes.
  virtual MVOID setRepeating(MBOOL const repeating);
  virtual MBOOL isRepeating() const;
  virtual std::string toString() const;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_APP_APPSTREAMBUFFERS_H_
