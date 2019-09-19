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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_COMMON_V4L2STREAMNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_COMMON_V4L2STREAMNODE_H_

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <Errors.h>
#include <cros-camera/v4l2_device.h>
#include <mtkcam/drv/def/IPostProcDef.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>

using cros::V4L2Buffer;
using cros::V4L2Device;
using cros::V4L2DevicePoller;
using cros::V4L2Format;
using cros::V4L2Subdevice;
using cros::V4L2VideoNode;

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v4l2 {

struct BufInfo {
  NSIoPipe::PortID mPortID;
  IImageBuffer* mBuffer;
  int mTransform;
  int magic_num;
  int64_t timestamp;
  int mRequestFD;    // represents RequestAPI fd
  int mSequenceNum;  // represents sequence number that driver filled
  int mSize;
  struct {
    NSIoPipe::MCropRect mCropRect;
    MSize mResizeDst;
  } FrameBased;
  //
  BufInfo()
      : mPortID(NSIoPipe::PORT_UNKNOWN),
        mBuffer(nullptr),
        mTransform(0),
        magic_num(0),
        timestamp(0),
        mRequestFD(0),
        mSequenceNum(0),
        mSize(0) {}
};

class VISIBILITY_PUBLIC V4L2StreamNode {
 public:
  /**
   * Node ID, the unique ID and bitwisable value.
   */
  enum ID {
    ID_UNKNOWN = 0,
    ID_P1_SUBDEV = 0x00000001,
    ID_P1_MAIN_STREAM = 0x00000002,
    ID_P1_SUB_STREAM = 0x00000004,
    ID_P1_META1 = 0x00000008,
    ID_P1_META2 = 0x00000010,
    ID_P1_META3 = 0x00000020,
    ID_P1_META4 = 0x00000040,
    ID_P1_TUNING = 0x00000080,
    ID_P2_SUBDEV = 0x00010000,
    ID_P2_RAW_INPUT = 0x00020000,
    ID_P2_TUNING = 0x00040000,
    ID_P2_VIPI = 0x00050000,
    ID_P2_LCEI = 0x00060000,
    ID_P2_MDP0 = 0x00080000,
    ID_P2_MDP1 = 0x00100000,
    ID_P2_IMG2 = 0x00110000,
    ID_P2_IMG3 = 0x00120000,
    ID_P2_CAP_SUBDEV = 0x00200000,
    ID_P2_CAP_RAW_INPUT = 0x00400000,
    ID_P2_CAP_TUNING = 0x00800000,
    ID_P2_CAP_VIPI = 0x00810000,
    ID_P2_CAP_LCEI = 0x00820000,
    ID_P2_CAP_MDP0 = 0x01000000,
    ID_P2_CAP_MDP1 = 0x02000000,
    ID_P2_CAP_IMG2 = 0x02100000,
    ID_P2_CAP_IMG3 = 0x02200000,
    ID_P2_REP_SUBDEV = 0x04000000,
    ID_P2_REP_RAW_INPUT = 0x08000000,
    ID_P2_REP_TUNING = 0x10000000,
    ID_P2_REP_VIPI = 0x11000000,
    ID_P2_REP_LCEI = 0x12000000,
    ID_P2_REP_MDP0 = 0x20000000,
    ID_P2_REP_MDP1 = 0x40000000,
    ID_P2_REP_IMG2 = 0x41000000,
    ID_P2_REP_IMG3 = 0x42000000
  };

 public:
  V4L2StreamNode(std::shared_ptr<V4L2VideoNode> node, std::string const name);
  virtual ~V4L2StreamNode();

  virtual status_t enque(BufInfo const& buf,
                         bool lazy_start = false,
                         std::shared_ptr<V4L2Subdevice> sub_device = nullptr);
  virtual status_t deque(BufInfo* p_buf);

  virtual status_t setFormatAnGetdBuffers(
      IImageBufferAllocator::ImgParam* imgParam,
      std::vector<std::shared_ptr<IImageBuffer> >* buffers);
  virtual status_t setBufFormat(IImageBufferAllocator::ImgParam* imgParam);
  virtual status_t setupBuffers();
  virtual status_t setBufPoolSize(int size);
  virtual status_t setActive(bool active, bool* changed);
  virtual status_t setControl(struct v4l2_control* control) {
    return mNode->SetControl(control->id, control->value);
  }
  virtual status_t getControl(struct v4l2_control* control) {
    return mNode->GetControl(control->id, &control->value);
  }
  virtual status_t start();
  virtual status_t stop();
  virtual const char* getName() const { return mName.c_str(); }
  virtual ID getId() const { return mId; }
  virtual std::shared_ptr<V4L2VideoNode> getVideoNode() const { return mNode; }
  virtual bool getActive() const { return mActive; }
  virtual int getBufferPoolSize() const { return mBufferPoolSize; }
  virtual bool isStart();
  virtual bool isPrepared();
  virtual bool isActive();

  enum StreamNodeState {
    CLOSED = 0, /*!< kernel device closed */
    OPEN,       /*!< device node opened */
    CONFIGURED, /*!< device format set, IOC_S_FMT */
    PREPARED,   /*!< device has requested buffers (set_buffer_pool)*/
    STARTED,    /*!< stream started, IOC_STREAMON */
    STOPED,     /*!< stream stop, IOC_STREAMOFF */
    ERROR       /*!< undefined state */
  };

  enum OutputPadNum { PAD_MDP0 = 4, PAD_MDP1 = 5, PAD_INVALID = 0 };

 public:
  /**
   * To check the given node_id was listened or not, according to bitmap
   * listened_nodes.
   */
  static inline bool is_listened(ID node_id, int listened_nodes) {
    static_assert(sizeof(decltype(node_id)) == sizeof(decltype(listened_nodes)),
                  "method 'is_listened' found data type sizes are different.");
    return !!(static_cast<int>(node_id) & listened_nodes);
  }

 private:
  virtual status_t setFormatLocked(
      MSize msize,
      int img_fmt,
      unsigned int color_profile,
      int sensor_order,
      IImageBufferAllocator::ImgParam* p_img_param = nullptr);
  virtual status_t startLocked();
  virtual status_t stopLocked();
  virtual status_t setupBuffersLocked();
  virtual status_t setTransformLocked(BufInfo const& buf);

 private:
  StreamNodeState mState;
  V4L2Format mFormat;
  std::string mName;
  v4l2_memory mType;
  std::mutex mOpLock;
  std::vector<V4L2Buffer> mBuffers;  // saves the buffers requested from driver
                                     // via VIDIOC_REQBUFS
  std::shared_ptr<V4L2VideoNode> mNode;

  std::map<int, V4L2Buffer*> mV4L2Buffers;
  std::map<int, NSCam::IImageBuffer*> mImageBuffers;
  std::map<int, int> mFds;
  std::map<int, void*> mMappAddrs;

  std::map<int, V4L2Buffer*> mUsedBuffers;  // represents the buffers in used
  std::map<int, V4L2Buffer*> mFreeBuffers;  // represents the available buffers

  // only for MMAP buffers, saves the exposed IImageBuffer* and related
  // V4L2Buffer* (alias of mBuffers).
  std::map<IImageBuffer*, V4L2Buffer*> mMMAPedImages;

  ID mId;
  int mBufferPoolSize;
  bool mActive;
  int mTransform;
};

}  // namespace v4l2
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_COMMON_V4L2STREAMNODE_H_
