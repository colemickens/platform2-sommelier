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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2STTPIPEMGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2STTPIPEMGR_H_

// MTKCAM
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IIOPipe.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>

// MTKCAM/V4L2
#include <mtkcam/def/common.h>
#include <mtkcam/v4l2/ipc_queue.h>
#include <mtkcam/v4l2/mtk_p1_metabuf.h>
#include <mtkcam/v4l2/V4L2DriverWorker.h>

// STL
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace v4l2 {

using NS3Av3::E3ACtrl_IPC_P1_Stt2Control;
using NS3Av3::E3ACtrl_IPC_P1_SttControl;
using NS3Av3::IHal3A;
using NS3Av3::IPC_Metabuf1_T;
using NS3Av3::IPC_Metabuf2_T;
using NSCam::IImageBuffer;
using NSCam::NSIoPipe::NSCamIOPipe::BufInfo;
using NSCam::NSIoPipe::NSCamIOPipe::PipeTag;
using NSCam::NSIoPipe::NSCamIOPipe::V4L2IIOPipe;

class VISIBILITY_PUBLIC V4L2SttPipeMgr : public V4L2DriverWorker {
 public:
  enum {
    DISABLE_META2 = 0,
    ENABLE_META2 = 1,
  };

 protected:
  /*
   * Statistic buffer info.
   *  @param sequence_num: the sequence_num that driver returned.
   *  @param fd: file descriptor of the current statistic buffer.
   */
  struct SttBufInfo {
    uint32_t sequence_num;
    void* fd;
    //
    SttBufInfo() : sequence_num(0) {}
    ~SttBufInfo() = default;
  };

  /*
   * Parcel to makes all image buffers are returned in sequential.
   *  @param pendingQueueLock: mutex of this parcel.
   *  @param pendingQueue: indexed-based queue, saves IImageBuffer* which were
   *                       pending to be enqued. The index is corresponding to
   *                       the v4l2 buffer(encapsulated by IImageBuffer) index.
   *  @param heap2idx_meta: IImageBuffer* address to buffer index map.
   *  @param pendingTarget: the index of next target to be enqued.
   */
  struct SeqCtrl {
    std::mutex pendingQueueLock;
    std::vector<IImageBuffer*> pendingQueue;
    std::map<IImageBuffer*, int> heap2idx_meta;
    std::atomic<int> pendingTarget;

    SeqCtrl() : pendingTarget(0) {}
    ~SeqCtrl() = default;
  };

  /*
   * template methods.
   */
 protected:
  /* get the related stt pipe instance based on BUF_TYPE */
  template <class BUF_TYPE>
  V4L2IIOPipe* getSttPipe();

  /* get the related port index based on BUF_TYPE */
  template <class BUF_TYPE>
  int getPortIndex() const;

  /* get the related sequence counter based on BUF_TYPE */
  template <class BUF_TYPE>
  std::atomic<uint32_t>& getSeqCnt();

  /* get sequential control object by BUF_TYPE */
  template <class BUF_TYPE>
  SeqCtrl& getSequentialControl();

  /*
   * constructor / destructor
   */
 public:
  V4L2SttPipeMgr(PipeTag pipe_tag, uint32_t sensorIdx, int enableMeta2);
  virtual ~V4L2SttPipeMgr();

  /*
   * re-implementations of V4L2DriverWorker
   */
 public:
  int start() override;
  int stop() override;

 protected:
  /**
   * After invoked V4L2DriverWorker::start, this method would be invoked
   * continuously, infinite loop, until invoked V4L2DriverWorker::stop.
   */
  void job() override;
  void job2();  // job2 is for controlling meta2 buffers

 private:
  void destroyV4L2IIOPipe();  // recycle V4L2IIOPipe (stt)
  int configurePipe();  // configure V4L2IIOPipe (Stt) and enque default buffer

  /* check the V4L2SttPipeMgr state */
  inline bool isValidState() const { return m_pSttPipe.get() != nullptr; }

  /*
   * Deque a buffer from driver, BUF_TYPE must be mtk_p1_metabuf_meta1 or
   * mtk_p1_metabuf_meta2.
   *  @tparam: mtk_p1_metabuf_meta1 or mtk_p1_metabuf_meta2.
   *  @param ppBuffer: The address of the given pointer.
   *  @param pInfo: The pointer of the SttBufInfo.
   *  @param timeoutms: Timeout in milliseconds of dequeuing.
   *  @return 0 for succeeded.
   */
  template <class BUF_TYPE>
  int dequeFromDrv(BUF_TYPE** ppBuffer, SttBufInfo* pInfo, size_t timeoutms);

  /*
   * Enque a buffer to driver, BUF_TYPE must be mtk_p1_metabuf_meta1 or
   * mtk_p1_metabuf_meta2.
   *  @tparam: mtk_p1_metabuf_meta1 or mtk_p1_metabuf_meta2.
   *  @param pBuffer: The pointer of the buffer.
   *  @return 0 for succeeded.
   */
  template <class BUF_TYPE>
  int enqueToDrv(BUF_TYPE* pBuffer);

  /*
   * Enque an IImageBuffer (aka v4l2 buffer) to driver, BUF_TYPE must be
   * mtk_p1_metabuf_meta1 or mtk_p1_metabuf_meta2.
   *  @tparam: mtk_p1_metabuf_meta1 or mtk_p1_metabuf_meta2.
   *  @param pImage: The pointer of the given IImageBuffer.
   *  @return 0 for succeeded.
   */
  template <class BUF_TYPE>
  int enqueIImageBufferToDrv(IImageBuffer* pImage);

  /*
   * fields
   */
 private:
  int m_sensorIdx;
  int m_logLevel;

  std::shared_ptr<IHal3A> m_pHal3A;
  std::shared_ptr<V4L2IIOPipe> m_pSttPipe;
  std::shared_ptr<V4L2IIOPipe> m_pSttPipe2;  // for meta2

  /* saves BufInfo, key is address of BufInfo::mBuffer->getVA(0) */
  std::map<void*, BufInfo> m_bufInfoMeta;
  std::mutex m_bufInfoLock;

  std::atomic<uint32_t> m_seqCnt1;
  std::atomic<uint32_t> m_seqCnt2;

  /* container saving buffers from driver */
  std::map<int, std::vector<std::shared_ptr<IImageBuffer>>> m_map_vbuffers;

  /* sub thread for deque meta2 */
  std::atomic<bool> m_bDequingMeta2;
  std::thread m_threadDeqMeta2;

  /* sequence control parcels */
  SeqCtrl m_seqCtrlMeta1;
  SeqCtrl m_seqCtrlMeta2;

  /* dequeue timeout, re-tried times */
  size_t m_dqErrCntMeta1;
  size_t m_dqErrCntMeta2;
};  // class V4L2SttPipeMgr
};  // namespace v4l2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2STTPIPEMGR_H_
