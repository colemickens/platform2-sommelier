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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_ISTREAMINFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_ISTREAMINFO_H_
//
#include <memory>
#include <string>
#include <vector>
//
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/**
 * Type of Camera Stream Id.
 */
typedef int64_t StreamId_T;

/**
 * Camera stream type declaration.
 */
enum EStreamType {
  //  Image Streams
  eSTREAMTYPE_IMAGE_OUT = 0u,  // Sync. to StreamType::OUTPUT
  eSTREAMTYPE_IMAGE_IN = 1u,   // Sync. to StreamType::INPUT
  eSTREAMTYPE_IMAGE_INOUT = 2u,
  eSTREAMTYPE_IMAGE_END_OF_TYPES,

  //  Metadata Streams
  eSTREAMTYPE_META_OUT,    // DYNAMIC
  eSTREAMTYPE_META_IN,     // CONTROL
  eSTREAMTYPE_META_INOUT,  // CONTROL/DYNAMIC
};

/**
 * An interface of stream info.
 */
class IStreamInfo {
 public:  ////                    Definitions.
  typedef NSCam::v3::StreamId_T StreamId_T;

 public:  ////                    Attributes.
  /**
   * A stream name.
   *
   * @remark This should be fixed and unchangable.
   */
  virtual char const* getStreamName() const = 0;

  /**
   * A unique stream ID.
   *
   * @remark This should be fixed and unchangable.
   */
  virtual StreamId_T getStreamId() const = 0;

  /**
   * A stream type of eSTREAMTYPE_xxx.
   *
   * @remark This should be fixed and unchangable.
   */
  virtual MUINT32 getStreamType() const = 0;

  /**
   * The maximum number of buffers which may be used at the same time.
   *
   * @remark This should be fixed and unchangable.
   */
  virtual size_t getMaxBufNum() const = 0;

  /**
   * Set the maximum number of buffers which may be used at the same time.
   *
   * @param[in] count: the max. number of buffers.
   */
  virtual MVOID setMaxBufNum(size_t count) = 0;

  /**
   * The minimum number of buffers which is suggested to allocate initially.
   *
   * @remark This should be fixed and unchangable.
   */
  virtual size_t getMinInitBufNum() const = 0;

 public:  ////                    Operations.
  /**
   * dump to a string for debug.
   */
  virtual std::string toString() const = 0;
};

/**
 * An interface of metadata stream info.
 */
class IMetaStreamInfo : public virtual IStreamInfo {
 public:  ////                    Definitions.
 public:  ////                    Attributes.
  /**
   * Query the maximum number of tags or sections.
   */
  virtual ~IMetaStreamInfo() {}
};

/**
 * An interface of image stream info.
 */
class IImageStreamInfo : public virtual IStreamInfo {
 public:  ////                    Definitions.
  /**
   * A single color plane of image buffer.
   */
  struct BufPlane {
    /**
     * The size for this color plane, in bytes.
     */
    size_t sizeInBytes;

    /**
     * The row stride for this color plane, in bytes.
     *
     * This is the distance between the start of two consecutive rows of
     * pixels in the image. The row stride is always greater than 0.
     */
    size_t rowStrideInBytes;
  };

  typedef std::vector<BufPlane> BufPlanes_t;

 public:  ////                    Attributes.
  /**
   * Usage for buffer consumer.
   *
   * @remark Both usages for allocator and consumer may have no intersection.
   */
  virtual MUINT64 getUsageForConsumer() const = 0;

  /**
   * Usage for buffer allocator.
   *
   * @remark Both usages for allocator and consumer may have no intersection.
   */
  virtual MUINT64 getUsageForAllocator() const = 0;

  /**
   * Image format.
   */
  virtual MINT getImgFormat() const = 0;

  /**
   * Image resolution, in pixels, without cropping.
   *
   * @remark This should be fixed and unchangable.
   */
  virtual MSize getImgSize() const = 0;

  /**
   * A vector of buffer planes.
   */
  virtual BufPlanes_t const& getBufPlanes() const = 0;

  /**
   * Get Image transform type.
   */
  virtual MUINT32 getTransform() const = 0;

  /**
   * Get Image seucre/normal memory type
   */
  virtual MBOOL getSecureInfo() const = 0;

  /**
   * Set Image transform type.
   */
  virtual MERROR setTransform(MUINT32 transform) = 0;
};

/**
 * An interface of stream info set.
 */
class IStreamInfoSet {
 public:  ////                            Definitions.
  template <class _IStreamInfoT_>
  struct IMap {
   public:  ////                        Operations.
    typedef _IStreamInfoT_ IStreamInfoT;

    virtual size_t size() const = 0;
    virtual ssize_t indexOfKey(StreamId_T id) const = 0;
    virtual std::shared_ptr<IStreamInfoT> valueFor(StreamId_T id) const = 0;
    virtual std::shared_ptr<IStreamInfoT> valueAt(size_t index) const = 0;
    virtual ~IMap() {}
  };

 public:  ////                            Operations.
  virtual std::shared_ptr<IMap<IMetaStreamInfo> > getMetaInfoMap() const = 0;
  virtual size_t getMetaInfoNum() const = 0;
  virtual std::shared_ptr<IMetaStreamInfo> getMetaInfoFor(
      StreamId_T id) const = 0;
  virtual std::shared_ptr<IMetaStreamInfo> getMetaInfoAt(
      size_t index) const = 0;

  virtual std::shared_ptr<IMap<IImageStreamInfo> > getImageInfoMap() const = 0;
  virtual size_t getImageInfoNum() const = 0;
  virtual std::shared_ptr<IImageStreamInfo> getImageInfoFor(
      StreamId_T id) const = 0;
  virtual std::shared_ptr<IImageStreamInfo> getImageInfoAt(
      size_t index) const = 0;
  virtual ~IStreamInfoSet() {}
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_ISTREAMINFO_H_
