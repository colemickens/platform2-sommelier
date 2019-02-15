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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_APP_CAM3IMAGESTREAMINFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_APP_CAM3IMAGESTREAMINFO_H_
//
#include <hardware/camera3.h>

#include <mtkcam/pipeline/stream/IStreamInfo.h>

#include <memory>
#include <string>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/**
 * camera3 image stream info.
 */
class Cam3ImageStreamInfo : public IImageStreamInfo {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Definitions.
  typedef Cam3ImageStreamInfo ThisT;

 protected:  ////                    Data Members.
  camera3_stream*
      mpCamera3Stream;  // camera3_stream::usage: usage for buffer allocator.
  std::string mStreamName;
  StreamId_T mStreamId;
  MUINT32 mStreamType;

  MUINT mUsageForConsumer;  // usage for buffer consumer.
  MINT mImgFormat;          // image format in reality.
  MINT mImgFormatToAlloc;   // image format for buffer allocation.
                            // It must equal to camera3_stream::format.
  MSize mImgSize;           // image size, in pixels.
                            // It must equal to width and height
                            // in camera3_strea
  BufPlanes_t mvbufPlanes;

  MUINT32 mTransform;

  MUINT32 mDataSpace;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Instantiation.
  /**
   *  @param[in] pCamera3Stream: pointer to a camera3_stream, where
   *      all fields except priv are specified.
   */
  Cam3ImageStreamInfo(camera3_stream* pCamera3Stream,
                      char const* streamName,
                      StreamId_T streamId,
                      MUINT usageForConsumer,
                      MUINT usageForAllocator,
                      MINT imgFormatToAlloc,
                      MINT imgFormatInFact,
                      BufPlanes_t const& bufPlanes,
                      size_t maxBufNum = 1,
                      MUINT32 transform = 0,
                      MUINT32 dataSpace = 0);
  virtual ~Cam3ImageStreamInfo() {}

 public:  ////                    Attributes.
  static std::shared_ptr<ThisT> cast(camera3_stream* stream);
  static std::shared_ptr<ThisT const> cast(camera3_stream const* stream);

  virtual camera3_stream* get_camera3_stream() const;

  /*
   * image format to alloc.
   *
   * @remark It must equal to camera3_stream::format.
   */
  virtual MINT getImgFormatToAlloc() const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IImageStreamInfo Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Attributes.
  /**
   * Usage for buffer consumer.
   */
  virtual MUINT64 getUsageForConsumer() const;

  /**
   * Usage for buffer allocator.
   *
   * @remark It must equal to camera3_stream::usage.
   */
  virtual MUINT64 getUsageForAllocator() const;

  virtual MINT getImgFormat() const;
  virtual MSize getImgSize() const;
  virtual BufPlanes_t const& getBufPlanes() const;
  virtual MUINT32 getTransform() const;
  virtual MERROR setTransform(MUINT32 transform);
  virtual MUINT32 getDataSpace() const;
  /**
   * Get Image seucre/normal memory type
   */
  virtual MBOOL getSecureInfo() const { return true; }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamInfo Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Attributes.
  virtual char const* getStreamName() const;
  virtual StreamId_T getStreamId() const;
  virtual MUINT32 getStreamType() const;
  virtual size_t getMaxBufNum() const;
  virtual MVOID setMaxBufNum(size_t count);
  virtual size_t getMinInitBufNum() const;
  virtual std::string toString() const;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_APP_CAM3IMAGESTREAMINFO_H_
