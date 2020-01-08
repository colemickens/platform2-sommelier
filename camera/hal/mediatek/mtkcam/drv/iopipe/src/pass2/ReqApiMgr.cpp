/* Copyright (C) 2019 MediaTek Inc.
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

#include <unistd.h>  // close(file descriptor)
#include <algorithm>
#include <cutils/compiler.h>
#include <map>

#include "mtkcam/drv/iopipe/src/pass2/ReqApiMgr.h"
#include "mtkcam/utils/std/Log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ReqApiMgr"

using NSCam::v4l2::ReqApiMgr;

ReqApiMgr::ReqApiMgr(std::weak_ptr<MtkCameraV4L2API> wp_v4l2api,
                     int media_dev_index,
                     void* p_nstream,
                     int fd_cnt)
    : mwp_v4l2api(wp_v4l2api), m_mediadev_idx(media_dev_index) {
  mv_records.reserve(fd_cnt *
                     2);  // reserve memory first, basically, we reserved
                          // much more memory to avoid memory reallocation.
  LOGI("create ReqApiMgr %p belonging nstream %p", this, p_nstream);

  reserve_requestapi_from_driver_locked(fd_cnt);
}

ReqApiMgr::~ReqApiMgr() {
  /* return all fds */
  for (const auto& el : mv_records) {
    if (CC_LIKELY(el.fd > 0)) {
      return_requestapi_to_driver_locked(el.fd);
    }
  }
}

ReqApiMgr::SYNC_ID ReqApiMgr::getSyncIdByNodeId(V4L2StreamNode::ID id) {
  /* RequestAPI support table */
  static std::map<V4L2StreamNode::ID, ReqApiMgr::SYNC_ID>
      k_syncid_nodeid_mapping_tbl = {
          {V4L2StreamNode::ID_P2_RAW_INPUT, ReqApiMgr::SYNC_ID_RAW_INPUT},
          {V4L2StreamNode::ID_P2_TUNING, ReqApiMgr::SYNC_ID_TUNING},
          {V4L2StreamNode::ID_P2_VIPI, ReqApiMgr::SYNC_ID_VIPI},
          {V4L2StreamNode::ID_P2_LCEI, ReqApiMgr::SYNC_ID_LCEI},
          {V4L2StreamNode::ID_P2_MDP0, ReqApiMgr::SYNC_ID_MDP0},
          {V4L2StreamNode::ID_P2_MDP1, ReqApiMgr::SYNC_ID_MDP1},
          {V4L2StreamNode::ID_P2_IMG2, ReqApiMgr::SYNC_ID_IMG2O},
          {V4L2StreamNode::ID_P2_IMG3, ReqApiMgr::SYNC_ID_IMG3O},
          {V4L2StreamNode::ID_P2_CAP_RAW_INPUT,
           ReqApiMgr::SYNC_ID_CAP_RAW_INPUT},
          {V4L2StreamNode::ID_P2_CAP_TUNING, ReqApiMgr::SYNC_ID_CAP_TUNING},
          {V4L2StreamNode::ID_P2_CAP_VIPI, ReqApiMgr::SYNC_ID_CAP_VIPI},
          {V4L2StreamNode::ID_P2_CAP_LCEI, ReqApiMgr::SYNC_ID_CAP_LCEI},
          {V4L2StreamNode::ID_P2_CAP_MDP0, ReqApiMgr::SYNC_ID_CAP_MDP0},
          {V4L2StreamNode::ID_P2_CAP_MDP1, ReqApiMgr::SYNC_ID_CAP_MDP1},
          {V4L2StreamNode::ID_P2_CAP_IMG2, ReqApiMgr::SYNC_ID_CAP_IMG2O},
          {V4L2StreamNode::ID_P2_CAP_IMG3, ReqApiMgr::SYNC_ID_CAP_IMG3O},
          {V4L2StreamNode::ID_P2_REP_RAW_INPUT,
           ReqApiMgr::SYNC_ID_REP_RAW_INPUT},
          {V4L2StreamNode::ID_P2_REP_TUNING, ReqApiMgr::SYNC_ID_REP_TUNING},
          {V4L2StreamNode::ID_P2_REP_VIPI, ReqApiMgr::SYNC_ID_REP_VIPI},
          {V4L2StreamNode::ID_P2_REP_LCEI, ReqApiMgr::SYNC_ID_REP_LCEI},
          {V4L2StreamNode::ID_P2_REP_MDP0, ReqApiMgr::SYNC_ID_REP_MDP0},
          {V4L2StreamNode::ID_P2_REP_MDP1, ReqApiMgr::SYNC_ID_REP_MDP1},
          {V4L2StreamNode::ID_P2_REP_IMG2, ReqApiMgr::SYNC_ID_REP_IMG2O},
          {V4L2StreamNode::ID_P2_REP_IMG3, ReqApiMgr::SYNC_ID_REP_IMG3O},
      };
  auto itr = k_syncid_nodeid_mapping_tbl.find(id);
  if (itr == k_syncid_nodeid_mapping_tbl.end()) {
    LOGI("(V4L2StreamNode::ID) node id %d doesn't support RequestAPI.", id);
    return SYNC_NONE;
  }
  return itr->second;
}

int ReqApiMgr::retainAvlReqApi() {
  std::lock_guard<std::mutex> lk(m_op_lock);

  if (CC_UNLIKELY(mq_record_freelist.empty() &&
                  !reserve_requestapi_from_driver_locked())) {
    LOGE(
        "there is no available record and reserve request api from driver "
        "fail");
    return -1;  // return illegal fd id for error return
  }

  auto record_idx = mq_record_freelist.front();
  mv_records[record_idx].occupation = true;
  mq_record_freelist.pop();

  return mv_records[record_idx].fd;
}

int ReqApiMgr::releaseUsedReqApi(SYNC_ID caller, int fd) {
  std::lock_guard<std::mutex> lk(m_op_lock);
  auto it_record = std::find_if(mv_records.begin(), mv_records.end(),
                                [fd](const Record& m) { return m.fd == fd; });

  /* check if not found */
  if (CC_UNLIKELY(it_record == mv_records.end())) {
    LOGE("caller(%#x) cannot found the given FD(%d), maybe something wrong",
         caller, fd);
    return -ENOENT;
  }

  /* check if it's not used yet, if not used yet, release is forbidden */
  if (CC_UNLIKELY(!it_record->occupation)) {
    LOGE(
        "the record(request_api=%d,caller=%#x) hasn't been used yet, "
        "cannot validate the magic number",
        it_record->fd, caller);
    return -EPERM;
  }

  /* debug: check if duplicated validations */
  if (CC_UNLIKELY(it_record->is_done(caller))) {
    LOGW("record(request_api=%d,caller=%#x) has been released already.",
         it_record->fd, caller);
    return 0;
  }

  it_record->mark_done(caller);

  if (it_record->is_done(static_cast<SYNC_ID>(it_record->user_id_mask))) {
    recycle_fd_locked(it_record->fd);
    it_record->reinit();
    mq_record_freelist.push(std::distance(
        mv_records.begin(), it_record));  // recycle record and push idx
                                          // to the tail of free list
    LOGD("reinit request_api:%#x and push idx:%d to free list",
         reinterpret_cast<int>(fd),
         std::distance(mv_records.begin(), it_record));
  }

  return 0;
}

int ReqApiMgr::notifyEnque(SYNC_ID sync_id, int fd) {
  std::lock_guard<std::mutex> lk(m_op_lock);

  std::shared_ptr<MtkCameraV4L2API> sp_api = mwp_v4l2api.lock();
  if (CC_UNLIKELY(sp_api.get() == nullptr)) {
    LOGE("cannot request a RequestAPI FD since no MtkCameraV4L2API instance");
    return -EFAULT;
  }

  auto it_record = std::find_if(mv_records.begin(), mv_records.end(),
                                [fd](const Record& m) { return m.fd == fd; });
  /* check if not found */
  if (CC_UNLIKELY(it_record == mv_records.end())) {
    LOGE("cannot found the given FD(%d), maybe something wrong", fd);
    return -ENOENT;
  }

  auto err = sp_api->queueRequest(m_mediadev_idx, fd);
  if (CC_UNLIKELY(err != 0)) {
    LOGE("queueRequest (request_api=%d) failed, errcode=%#x", fd, err);
    return err;
  }

  it_record->user_id_mask = sync_id;

  LOGD("MEDIA_REQUEST_IOC_QUEUE: user=%#x request_api=%d", sync_id, fd);

  return 0;
}

bool ReqApiMgr::reserve_requestapi_from_driver_locked(size_t count) {
  for (size_t i = 0; i < count; ++i) {
    int fd = request_fd_locked();
    if (CC_UNLIKELY(fd <= 0)) {
      LOGE("request a FD by RequestAPI failed, got nullptr.(i=%zu)", i);
      return false;
    }
    /* push back into m_records */
    Record r;
    r.fd = fd;
    mv_records.emplace_back(r);
    mq_record_freelist.push(mv_records.size() -
                            1);  // push idx of avail record to list
  }

  LOGD("m_records grows up to size =%zu", mv_records.size());
  return true;
}

void ReqApiMgr::return_requestapi_to_driver_locked(int fd) {
  /* close Request API just like closing a file descriptor */
  LOGD("close request api=%d", fd);
  close(fd);
}

int ReqApiMgr::request_fd_locked() {
  std::shared_ptr<MtkCameraV4L2API> sp_api = mwp_v4l2api.lock();
  if (CC_UNLIKELY(sp_api.get() == nullptr)) {
    LOGE("cannot request a RequestAPI FD since no MtkCameraV4L2API instance");
    return -1;  // return illegal fd id for error return
  }

  int request_fd = 0;

  auto err = sp_api->allocateRequest(m_mediadev_idx, &request_fd);
  if (CC_UNLIKELY(err != 0)) {
    LOGW("allocateRequest returns error(code=%#x)", err);
  }

  LOGD("allocateRequest request_api=%d", request_fd);
  return request_fd;
}

void ReqApiMgr::recycle_fd_locked(int fd) {
  std::shared_ptr<MtkCameraV4L2API> sp_api = mwp_v4l2api.lock();
  if (CC_UNLIKELY(sp_api.get() == nullptr)) {
    LOGE("cannot recycle a RequestAPI FD since no MtkCameraV4L2API instance");
    return;
  }

  auto err = sp_api->reInitRequest(m_mediadev_idx, fd);
  if (CC_UNLIKELY(err != 0)) {
    LOGE("reInitRequest returns error(code=%d)", fd);
    return;
  }
}
