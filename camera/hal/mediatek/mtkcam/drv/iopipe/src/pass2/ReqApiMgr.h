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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS2_REQAPIMGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS2_REQAPIMGR_H_

// mtkcam
#include <V4L2StreamNode.h>
#include <MtkCameraV4L2API.h>
#include <memory>
#include <queue>
#include <vector>
// cros
#include <cros-camera/v4l2_device.h>

/** The default value of the count of reserved RequestAPI fd */
constexpr unsigned int k_reqapimgr_def_reserved_fd_cnt = 16;

namespace NSCam {
namespace v4l2 {

class ReqApiMgr {
 public:
  ReqApiMgr(std::weak_ptr<MtkCameraV4L2API> wp_v4l2api,
            int media_dev_index,
            void* p_nstream,
            int fd_cnt = k_reqapimgr_def_reserved_fd_cnt);
  virtual ~ReqApiMgr();

  enum SYNC_ID {
    SYNC_NONE = 0,
    SYNC_ID_RAW_INPUT = 1 << 0,
    SYNC_ID_TUNING = 1 << 1,
    SYNC_ID_MDP0 = 1 << 2,
    SYNC_ID_MDP1 = 1 << 3,
    SYNC_ID_CAP_RAW_INPUT = 1 << 4,
    SYNC_ID_CAP_TUNING = 1 << 5,
    SYNC_ID_CAP_MDP0 = 1 << 6,
    SYNC_ID_CAP_MDP1 = 1 << 7,
    SYNC_ID_VIPI = 1 << 8,
    SYNC_ID_LCEI = 1 << 9,
    SYNC_ID_IMG3O = 1 << 10,
    SYNC_ID_IMG2O = 1 << 11
  };
  /**
   *  @par Description
   *      Retrieve SYNC_ID by V4L2StreamNode::ID
   *  @param [in]
   *      id : v4l2_streamnode id
   *  @return
   *      ReqApiMgr::SYNC_ID
   */
  ReqApiMgr::SYNC_ID getSyncIdByNodeId(V4L2StreamNode::ID id);
  /**
   *  @par Description
   *      get available request api if no free, function will reserve request
   * api from driver.
   *  @par Parameters
   *      none
   *  @return
   *      pointer of fd, if it fails then return nullptr.
   */
  int retainAvlReqApi();
  /**
   *  @par Description
   *      release request api after deque successfully
   *  @par [in]
   *      caller : sync id of request api user.
   *      fd : the fd
   *  @return
   *      0 : success, others : should refer to error code
   */
  int releaseUsedReqApi(SYNC_ID caller, int fd);
  /**
   *  @par Description
   *      Notify ReqApiMgr that the specified caller has invoked ioctl
   *      VIDIOC_QBUF.
   *  @par [in]
   *      sync_id : users contained in this request api will be sent to driver
   *      fd : the fd
   *  @return
   *      0 : success, others : should refer to error code
   */
  int notifyEnque(SYNC_ID sync_id, int fd);

 protected:
  struct Record {
    int fd = -1;
    bool occupation =
        false;  // true: occupied by user, false: this record is available
    int user_id_mask =
        0;  // represent which user is belong to this requqest api.
    uint32_t status_done =
        0;  // represents done status. 0: not finished yet, 1: finished usage.

    bool is_done(SYNC_ID c) const { return (status_done & c) == c; }

    void mark_done(SYNC_ID c) { status_done |= c; }

    void reinit() {  // do reinit after recycle request api
      user_id_mask = 0;
      status_done = 0;
      occupation = false;
    }
  };

  /**
   *  @par Description
   *      Reserves N RequestAPI fds from driver, and add them into m_records.
   *  @param [int]
   *      count : Amount of fds
   *  @return
   *      true means success; false means fail
   */
  bool reserve_requestapi_from_driver_locked(size_t count = 1);

  /**
   *  @par Description
   *      Return the RequestAPI file descriptor to driver.
   *  @param [in]
   *      fd : The fd.
   *  @return
   *      none
   */
  void return_requestapi_to_driver_locked(int fd);

  /**
   *  @par Description
   *      Request a RequestAPI fd from driver, if this method failed, returns
   *      nullptr.
   *  @par params
   *      none
   *  @return
   *      Fd from driver.
   */
  int request_fd_locked();

  /**
   *  @par Description
   *      Recycle a RequestAPI.
   *  @par [in]
   *      fd : RequestAPI fd.
   *  @return
   *      none.
   */
  void recycle_fd_locked(int fd);

 private:
  std::weak_ptr<MtkCameraV4L2API> mwp_v4l2api;
  int m_mediadev_idx;
  std::vector<Record> mv_records;
  std::mutex m_op_lock;
  std::queue<unsigned int> mq_record_freelist;
};
}  // namespace v4l2
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS2_REQAPIMGR_H_
