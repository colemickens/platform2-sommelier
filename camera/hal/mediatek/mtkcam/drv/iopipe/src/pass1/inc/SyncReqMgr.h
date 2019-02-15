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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_SYNCREQMGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_SYNCREQMGR_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

// mtkcam
#include <V4L2StreamNode.h>
#include <MtkCameraV4L2API.h>

// cros
#include <cros-camera/v4l2_device.h>

/** The default value of the count of reserved RequestAPI fd */
#define SYNCREQMGR_DEF_RESERVED_FD_CNT 16

namespace NSCam {
namespace v4l2 {

/**
 * Synchronous Request Manager (SyncReqMgr) is a module that provide MTK camera
 * HAL "The Magic Number Mechanism", which is a frame sync mechanism of the
 * legacy MTK camera HAL. SyncReqMgr encapsulated V4L2 RequestAPI
 * implementations and made V4L2 RequestAPI be transparent to MTK camera HAL.
 *
 *  @note This class is reentrant, and thread-safe.
 */
class SyncReqMgr {
 public:
  enum SYNC_ID {
    SYNC_NONE = 0,
    SYNC_P1NODE = 1 << 0,
    SYNC_MAIN_STREAM = 1 << 1,
    SYNC_SUB_STREAM = 1 << 2,
    SYNC_META1 = 1 << 3,
    SYNC_META2 = 1 << 4,
    SYNC_META3 = 1 << 5,
    SYNC_META4 = 1 << 6,
    SYNC_TUNING = 1 << 7,
  };

  /** Retrieve SYNC_ID by V4L2StreamNode::ID */
  static SYNC_ID getSyncIdByNodeId(V4L2StreamNode::ID id);

 protected:
  /**
   * A record saves the magic number, RequestAPI fd and status. This structure
   * is not public, it's used by SyncReqMgr itself.
   */
  struct Record {
    uint32_t magicnum;
    int fd;
    /* status */
    uint32_t status_inusing;  // bitmask of in-using status. 0: not use yet, 1:
                              // in-using or done.
    uint32_t status_done;     // bitmask of done status. 0: not finished yet, 1:
                              // finished usage.
    uint32_t status_notified;  // bitmask of notified enqueued status. 0: not
                               // yet, 1: notified.
    //
    Record()
        : magicnum(0),
          fd(-1),
          status_inusing(0),
          status_done(0),
          status_notified(0) {}
    //
    inline bool is_free(SYNC_ID c) const { return (status_inusing & c) == 0; }

    inline bool is_inusing(SYNC_ID c) const { return !is_free(c); }

    inline void mark_inused(SYNC_ID c) { status_inusing |= c; }

    inline bool is_done(SYNC_ID c) const { return (status_done & c) == c; }

    inline void mark_done(SYNC_ID c) { status_done |= c; }

    inline void mark_notified(SYNC_ID c) { status_notified |= c; }

    inline bool is_notified(SYNC_ID c) { return (status_notified & c) == c; }
  };

  //
  // public methods
  //
 public:
  /**
   * Acquire the magic number by the given magic number.
   *  @param caller       Caller ID.
   *  @param fd           The given RequestAPI FD for look up.
   *  @return             The magic number.
   *  @complexity         N.
   *  @note After invoked this method, the record would be marked as in-used.
   */
  uint32_t acquireMagicNum(SYNC_ID caller, int fd);

  /**
   * Acquire the RequestAPI FD by the given magic number.
   *  @param caller       Caller ID.
   *  @param magicnum     The given magicnum for look up.
   *  @return             The RequestAPI FD, if the given magic number doesn't
   *                      exist, this method returns nullptr.
   *  @complexity         N.
   *  @note After invoked this method, the record would be marked as in-used.
   */
  int acquireRequestAPI(SYNC_ID caller, uint32_t magicnum);

  /**
   * Acquire the available magic number (FIFO).
   *  @param caller       Caller ID.
   *  @return             The magic number.
   *  @complexity         N.
   *  @note After invoked this method, the record would be marked as in-used.
   */
  uint32_t acquireAvailableMagicNum(SYNC_ID caller);

  /**
   * Validate the magic number from the given FD from RequestAPI.
   *  @param caller       Caller ID.
   *  @return             The magic number.
   *  @complexity         N.
   *  @note After invoked this method, the record would be marked as done.
   */
  uint32_t validateMagicNum(SYNC_ID caller, int fd);

  /**
   * Validate the RequestAPI fd from the given magic number.
   *  @param caller       Caller ID.
   *  @param magicnum     The given magic number to look up for.
   *  @return             The related RequestAPI fd, if the magicnum doesn't
   *                      exist, or the record has already been validated, this
   *                      method returns nullptr.
   *  @complexity         N.
   *  @note After invoked this method, the record would be marked as done.
   */
  int validateRequestAPI(SYNC_ID caller, uint32_t magicnum);

  /**
   * Notify SyncReqMgr that the specified caller has invoked ioctl
   * VIDIOC_QBUF. When all callers are notified SyncReqMgr, the ioctl
   * MEDIA_REQUEST_IOC_QUEUE will be invoked to the related RequestAPI fd.
   *  @param caller       Caller ID.
   *  @param magicnum     The request's magic number.
   *  @return             Returns 0 if ok, otherwise checks the error code.
   *  @note After invoked this method, the record would be marked as notified.
   */
  int notifyEnqueuedByMagicNum(SYNC_ID caller, uint32_t magicnum);

  /**
   * Notify SyncReqMgr that the specified caller has invoked ioctl
   * VIDIOC_QBUF. When all callers are notified SyncReqMgr, the ioctl
   * MEDIA_REQUEST_IOC_QUEUE will be invoked to the related RequestAPI fd.
   *  @param caller       Caller ID.
   *  @param fd           RequestAPI fd.
   *  @return             Returns 0 if ok, otherwise checks the error code.
   *  @note After invoked this method, the record would be marked as notified.
   */
  int notifyEnqueuedByRequestAPI(SYNC_ID caller, int fd);

  //
  // inline public methods
  //
 public:
  /**
   * Get the enabled users SYNC_ID. Basically, caller uses this method to
   * retrieve the bit mask which represents the modules which are using
   * RequestAPI.
   *  @return             Bit mask of SYNC_ID.
   */
  inline uint32_t getAllUsers() const { return m_useridMask; }

  /**
   * Check if the given SYNC_ID has enabled RequestAPI or not.
   *  @param sync_id      The SYNC_ID.
   *  @return             true for enabled, false for not.
   */
  inline bool isEnableRequestAPI(SYNC_ID sync_id) const {
    return !!(sync_id & m_useridMask);
  }

  //
  // implementations of SyncReqMgr
  //
 protected:
  /**
   * Reserves N RequestAPI fds from driver, and add them into m_records.
   *  @param count        Amount of fds.
   */
  void reserve_requestapi_from_driver_locked(size_t count);

  /**
   * Acquire a RequestAPI fds from driver, and return the current magic number.
   *  @return             The magic number, if this method failed,
   * m_records.end() will be returned.
   */
  std::vector<Record>::iterator
  ask_requestapi_from_driver_locked();  // acquire Request API fd from v4l2
                                        // driver.

  /**
   * Return the RequestAPI file descriptor to driver.
   *  @param fd           The fd.
   */
  void return_requestapi_to_driver_locked(int fd);

  /**
   * Request a RequestAPI fd from driver, if this method failed, returns
   * nullptr.
   *  @return             Fd from driver.
   */
  int request_fd_locked();

  /**
   * Recycle a RequestAPI.
   *  @param fd           RequestAPI fd.
   */
  void recycle_fd_locked(int fd);

  /**
   * Invoke MEDIA_REQUEST_IOC_QUEUE ioctl.
   *  @param fd           The RequestAPI file descriptor.
   *  @param magicnum     The magic number for debug.
   *  @return             Returns 0 for ok, otherwise checks error.h
   */
  int ioctl_media_request_queue_locked(int fd, uint32_t magicnum);

 public:
  /**
   * Create a SyncReqMgr, it needs a weak pointer of MtkCameraV4L2API control;
   * a bitset users which represents all users we need to monitor; the index
   * of the target V4L2 Media Device, and fd_cnt
   * (default is SYNCREQMGR_DEF_RESERVED_FD_CNT) to indicate default RequestAPI
   * fd count that SyncReqMgr is going to acquire.
   *  @param p_v4l2api        Reference of MtkCameraV4L2API instance.
   *  @param media_dev_index  The index of P1 Media Device.
   *  @param users            User ID mask, the users we care.
   *  @param fd_cnt           RequestAPI fd count to reserve while
   * initialization.
   */
  SyncReqMgr(std::weak_ptr<MtkCameraV4L2API> p_v4l2api,
             int media_dev_index,
             int users,
             int fd_cnt = SYNCREQMGR_DEF_RESERVED_FD_CNT);

  virtual ~SyncReqMgr();

  // attributes
 private:
  int m_useridMask;
  int m_mediadev_idx;
  std::atomic<uint32_t> m_magicNum;
  //
  std::weak_ptr<MtkCameraV4L2API> m_pV4L2Api;
  //
  std::vector<Record> m_records;  // CPU cache may boost up operations
  std::mutex m_opLock;
};      // class SyncReqMgr
};      // namespace v4l2
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_SYNCREQMGR_H_
