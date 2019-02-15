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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS2_NORMALSTREAM_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS2_NORMALSTREAM_H_
//

#include <MtkCameraV4L2API.h>
#include <MediaEntity.h>

#include "INormalStream.h"
#include "PollerThread.h"
#include "ReqApiMgr.h"

#include <bitset>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <mtkcam/def/common.h>

using NSCam::NSIoPipe::EStreamPipeID_Normal;
using NSCam::NSIoPipe::QParams;

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v4l2 {

class MessagePollEvent {
 public:
  int requestId;
  std::shared_ptr<V4L2VideoNode>* activeDevices;
  size_t polledDevices;
  size_t numDevices;
  IPollEventListener::PollEventMessageId pollMsgId;

  MessagePollEvent()
      : requestId(-1),
        polledDevices(0),
        numDevices(0),
        pollMsgId(IPollEventListener::POLL_EVENT_ID_ERROR) {}
};

/******************************************************************************
 *
 * @struct FramePackage
 * @brief frame package for enqueue/dequeue (one package may contain multiple
 *frames)
 * @details
 *
 ******************************************************************************/
typedef std::bitset<16> FrameBitSet;

class FramePackage {
 public:
  FramePackage();
  explicit FramePackage(const QParams& params);
  status_t updateFrame(IImageBuffer* const& frame);
  bool checkFrameDone();

  QParams mParams;
  std::map<int, FrameBitSet> mDequeBitSet;
};

struct DmaConfigHeader {
  MUINT8 enable;
  MUINT32 port_index;
  MINT32 img_fmt;
  MINT32 img_width;
  MINT32 img_height;
  MUINT32 bit_per_pixel;  // size_t
  MUINT32 buf_iova;
  MUINT32 buf_size_bytes;    // size_t
  MUINT32 buf_stride_bytes;  // size_t
  MUINT32 buf_stride_pixel;  // size_t
  //
  DmaConfigHeader()
      : enable(0),
        port_index(0),
        img_fmt(-1),
        img_width(-1),
        img_height(-1),
        bit_per_pixel(0),
        buf_iova(0),
        buf_size_bytes(0),
        buf_stride_bytes(0),
        buf_stride_pixel(0) {}
};

class VISIBILITY_PUBLIC NormalStream : public INormalStream,
                                       public IPollEventListener {
  friend class INormalStream;
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipe Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  NormalStream(MUINT32 openedSensorIndex,
               NSCam::NSIoPipe::EStreamPipeID mPipeID = EStreamPipeID_Normal);

 public:
  virtual ~NormalStream();

 public:
  /**
   * @brief init the pipe
   *
   * @details
   *
   * @note
   *
   * @param[in] szCallerName: caller name.
   *            mPipeID     : Stream PipeID
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL init(char const* szCallerName,
                     ENormalStreamTag streamTag = ENormalStreamTag_Normal,
                     bool hasTuning = true);

  virtual MBOOL init(
      char const* szCallerName,
      const StreamConfigure config,
      NSCam::v4l2::ENormalStreamTag streamTag = ENormalStreamTag_Normal,
      bool hasTuning = true);
  /**
   * @brief uninit the pipe
   *
   * @details
   *
   * @note
   *
   * @param[in] szCallerName: caller name.
   *            mPipeID     : Stream PipeID
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL uninit(char const* szCallerName);
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Buffer Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  /**
   * @brief En-queue a request into the pipe.
   *
   * @details
   *
   * @note
   *
   * @param[in] rParams: Reference to a request of QParams structure.
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by \n
   *   getLastErrorCode().
   */
  virtual MBOOL enque(QParams* pParams);
  /**
   * @brief De-queue a result from the pipe.
   *
   * @details
   *
   * @note
   *
   * @param[in] rParams: Reference to a result of QParams structure.
   *
   * @param[in] i8TimeoutNs: timeout in nanoseconds \n
   *      If i8TimeoutNs > 0, a timeout is specified, and this call will \n
   *      be blocked until a result is ready. \n
   *      If i8TimeoutNs = 0, this call must return immediately no matter \n
   *      whether any buffer is ready or not. \n
   *      If i8TimeoutNs = -1, an infinite timeout is specified, and this call
   *      will block forever.
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL deque(QParams* p_rParams, MINT64 i8TimeoutNs = -1);

  virtual MBOOL requestBuffers(
      int type,
      IImageBufferAllocator::ImgParam imgParam,
      std::vector<std::shared_ptr<IImageBuffer>>* p_buffers);

  /**
   * @brief send isp extra command
   *
   * @details
   *
   * @note
   *
   * @param[in] cmd: command
   * @param[in] arg: arg
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * sendCommand().
   */
  virtual MBOOL sendCommand(ESDCmd cmd,
                            MINTPTR arg1 = 0,
                            MINTPTR arg2 = 0,
                            MINTPTR arg3 = 0);

  // IPollEvenListener
  virtual status_t notifyPollEvent(PollEventMessage* msg);

  virtual status_t validNode(int type, std::shared_ptr<V4L2StreamNode>* p_node);

 private:
  virtual status_t _applyPortPolicy(MSize img_size,
                                    MINT img_fmt,
                                    MUINT32 img_rotate,
                                    bool input,
                                    int largest_w,
                                    int* port);
  virtual status_t _find_format_and_erase(MSize img_size,
                                          MINT img_fmt,
                                          MINT32 img_rot,
                                          int port);
  virtual MBOOL _is_swap_width_height(int transform);
  virtual status_t _set_meta_buffer(int port,
                                    IImageBuffer* dst_buffer,
                                    IImageBuffer* src_buffer);
  virtual MBOOL _is_single_output(int streamTag);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Variables.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  //
  static std::mutex mOpenLock;
  static std::atomic<int> mUserCount;
  mutable std::mutex mLock;
  std::condition_variable mCondition;
  int mStreamTag;
  int mMediaDevice;
  bool mFirstFrame;
  MediaDeviceTag mDeviceTag;
  std::shared_ptr<MtkCameraV4L2API> mControl;
  std::shared_ptr<V4L2Subdevice> mSubDevice;
  std::unique_ptr<PollerThread> mPoller;
  std::vector<std::shared_ptr<MediaEntity>> mMediaEntity;
  std::map<int, std::shared_ptr<V4L2StreamNode>> mNodes;
  std::vector<std::shared_ptr<V4L2StreamNode>> mAllNodes;
  std::map<const std::string, std::shared_ptr<V4L2StreamNode>> mDeviceFdToNode;
  std::map<int, IImageBufferAllocator::ImgParam> mPortIdxToFmt;
  std::map<int, IImageBufferAllocator::ImgParam> mMdpIdxToFmt;
  std::vector<std::shared_ptr<IImageBuffer>> mRequestedBuffers;
  std::map<int, std::shared_ptr<V4L2StreamNode>> mFmtKeyToNode;

  mutable std::mutex mQueueLock;
  mutable std::mutex mDeQueueLock;
  std::deque<FramePackage> mFrameQueue;
  std::deque<FramePackage> mDeFrameQueue;
  std::unique_ptr<ReqApiMgr> mupReqApiMgr;
  std::string mStreamName;
};
/******************************************************************************
 *
 ******************************************************************************/
};      // namespace v4l2
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS2_NORMALSTREAM_H_
