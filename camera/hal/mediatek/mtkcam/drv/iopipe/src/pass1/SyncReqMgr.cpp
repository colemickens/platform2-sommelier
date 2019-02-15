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

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "SyncReqMgr"

#include "inc/SyncReqMgr.h"
#include "mtkcam/utils/std/Log.h"
// LINUX
#include <unistd.h>  // close(file descriptor)

// STL
#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include <cutils/compiler.h>

namespace NSCam {
namespace v4l2 {

SyncReqMgr::SYNC_ID SyncReqMgr::getSyncIdByNodeId(V4L2StreamNode::ID id) {
  /* RequestAPI support table */
  static std::map<V4L2StreamNode::ID, SyncReqMgr::SYNC_ID>
      s_syncid_nodeid_mapping_tbl = {
          {V4L2StreamNode::ID_P1_MAIN_STREAM, SyncReqMgr::SYNC_MAIN_STREAM},
          {V4L2StreamNode::ID_P1_SUB_STREAM, SyncReqMgr::SYNC_SUB_STREAM},
          {V4L2StreamNode::ID_P1_META3, SyncReqMgr::SYNC_META3},
          {V4L2StreamNode::ID_P1_META4, SyncReqMgr::SYNC_META4},
          {V4L2StreamNode::ID_P1_TUNING, SyncReqMgr::SYNC_TUNING}};

  auto itr = s_syncid_nodeid_mapping_tbl.find(id);
  if (itr == s_syncid_nodeid_mapping_tbl.end()) {
    return SYNC_NONE;
  }
  return itr->second;
}

SyncReqMgr::SyncReqMgr(std::weak_ptr<MtkCameraV4L2API> p_v4l2api,
                       int media_dev_index,
                       int users,
                       int fd_cnt /* = SYNCREQMGR_DEF_RESERVED_FD_CNT */)
    : m_useridMask(users),
      m_mediadev_idx(media_dev_index),
      m_magicNum(1),  // start from 1
      m_pV4L2Api(p_v4l2api) {
  m_records.reserve(fd_cnt *
                    2);  // reserve memory first, basically, we reserved
                         // much more memory to avoid memory reallocation.
  MY_LOGI("created SyncReqMgr %p, callers=%#x", this, users);

  reserve_requestapi_from_driver_locked(fd_cnt);
}

SyncReqMgr::~SyncReqMgr() {
  /* return all fds */
  for (const auto& el : m_records) {
    if (CC_LIKELY(el.fd > 0)) {
      return_requestapi_to_driver_locked(el.fd);
    }
  }
}

uint32_t SyncReqMgr::acquireMagicNum(SYNC_ID caller, int fd) {
  std::lock_guard<std::mutex> lk(m_opLock);
  auto itr = std::find_if(m_records.begin(), m_records.end(),
                          [fd](const Record& el) { return el.fd == fd; });

  /* not found */
  if (CC_UNLIKELY(itr == m_records.end())) {
    MY_LOGD("caller(%#x), recrod not found (by request_api=%d)", caller, fd);
    return 0;
  }

  itr->mark_inused(caller);  // mark as in-used.
  return itr->magicnum;
}

int SyncReqMgr::acquireRequestAPI(SYNC_ID caller, uint32_t magicnum) {
  std::lock_guard<std::mutex> lk(m_opLock);
  auto itr = std::find_if(
      m_records.begin(), m_records.end(),
      [&magicnum](const Record& el) { return el.magicnum == magicnum; });

  /* not found */
  if (CC_UNLIKELY(itr == m_records.end())) {
    MY_LOGD("caller(%#x), recrod not found (by magicnum=%u)", caller, magicnum);
    return 0;
  }

  /* dbg: check if already in-used */
  if (CC_UNLIKELY(!itr->is_free(caller))) {
    MY_LOGW("recrod(m=%u,request_api=%d,caller=%#x) is already in-using.",
            itr->magicnum, itr->fd, caller);
  }

  itr->mark_inused(caller);
  return itr->fd;
}

uint32_t SyncReqMgr::acquireAvailableMagicNum(SYNC_ID caller) {
  std::lock_guard<std::mutex> lk(m_opLock);
  /* find the first record which is free of the given caller */
  for (auto&& r : m_records) {
    /* if found, marked as inusing, and return the magic number */
    if (r.is_free(caller)) {
      r.mark_inused(caller);
      return r.magicnum;
    }
  }

  /* no magic number (RequestAPI) can be used, acquire one from driver */
  auto itr = ask_requestapi_from_driver_locked();
  if (CC_UNLIKELY(itr == m_records.end())) {
    MY_LOGE("caller(%#x): ask_requestapi_from_driver_locked failed", caller);
    return 0;
  }
  itr->mark_inused(caller);
  return itr->magicnum;
}

uint32_t SyncReqMgr::validateMagicNum(SYNC_ID caller, int fd) {
  uint32_t r_magicnum = 0;

  std::lock_guard<std::mutex> lk(m_opLock);
  auto itr = std::find_if(m_records.begin(), m_records.end(),
                          [fd](const Record& el) { return el.fd == fd; });

  /* check if not found */
  if (CC_UNLIKELY(itr == m_records.end())) {
    MY_LOGE("caller(%#x) cannot found the given FD(%d), maybe something wrong",
            caller, fd);
    return 0;
  }

  /* check if it's not used yet, if not used yet, this method should return 0 */
  if (CC_UNLIKELY(itr->is_free(caller))) {
    MY_LOGE(
        "the record(m=%u,request_api=%d,caller=%#x) hasn't been used yet, "
        "cannot validate the magic number",
        itr->magicnum, itr->fd, caller);
    return 0;
  }

  /* debug: check if duplicated validations */
  if (CC_UNLIKELY(itr->is_done(caller))) {
    MY_LOGW(
        "recrod(m=%u,request_api=%d,caller=%#x) has been validated already.",
        itr->magicnum, itr->fd, caller);
  }

  /* copy magic number */
  r_magicnum = itr->magicnum;

  /* mark as done */
  itr->mark_done(caller);

  /*
   * check if all caller has finished, if yes, remove this record,
   * and push back a new one but reuse RequestAPI fd.
   */
  if (itr->is_done((SYNC_ID)getAllUsers())) {
    Record r;
    r.fd = itr->fd;  // re-use the RequestAPI fd
    r.magicnum = m_magicNum++;

    /* recycle RequestAPI */
    recycle_fd_locked(r.fd);

    MY_LOGD("all done, reused request_api=%#x", reinterpret_cast<int>(r.fd));

    /* remove the current record, and push back a new one */
    m_records.erase(itr);
    m_records.emplace_back(r);
  }

  return r_magicnum;
}

int SyncReqMgr::validateRequestAPI(SYNC_ID caller, uint32_t magicnum) {
  std::lock_guard<std::mutex> lk(m_opLock);
  /* FIFO */
  auto itr = std::find_if(
      m_records.begin(), m_records.end(),
      [&magicnum](const Record& el) { return el.magicnum == magicnum; });

  /* check if not found */
  if (CC_UNLIKELY(itr == m_records.end())) {
    MY_LOGE("caller(%#x) cannot found the magic num(%u), maybe something wrong",
            caller, magicnum);
    return 0;
  }

  /* check if it's not used yet, if not used yet, this method should return 0 */
  if (CC_UNLIKELY(itr->is_free(caller))) {
    MY_LOGE(
        "the record(m=%u,request_api=%d,caller=%#x) hasn't been used yet, "
        "cannot validate the magic number",
        itr->magicnum, itr->fd, caller);
    return 0;
  }

  /* debug: check if duplicated validations */
  if (CC_UNLIKELY(itr->is_done(caller))) {
    MY_LOGW(
        "recrod(m=%u,request_api=%d,caller=%#x) has been validated already.",
        itr->magicnum, itr->fd, caller);
  }

  /* mark as done */
  itr->mark_done(caller);

  /*
   * check if all caller has finished, if yes, remove this record,
   * and push back a new one but reuse RequestAPI fd.
   */
  if (itr->is_done((SYNC_ID)getAllUsers())) {
    Record r;
    r.fd = itr->fd;             // re-use the RequestAPI fd
    r.magicnum = m_magicNum++;  // increase magic number counting

    /* recycle RequestAPI */
    recycle_fd_locked(r.fd);

    MY_LOGD("all done, reused request_api=%#x", reinterpret_cast<int>(r.fd));

    /* remove the current record, and push back a new one */
    m_records.erase(itr);
    m_records.emplace_back(r);
  }

  return itr->fd;
}

int SyncReqMgr::notifyEnqueuedByMagicNum(SYNC_ID caller, uint32_t magicnum) {
  int err = 0;

  std::lock_guard<std::mutex> lk(m_opLock);
  auto itr = std::find_if(
      m_records.begin(), m_records.end(),
      [magicnum](const Record& el) { return el.magicnum == magicnum; });
  if (CC_UNLIKELY(itr == m_records.end())) {
    MY_LOGE("caller(%#x) cannot found the magic num(%u), maybe something wrong",
            caller, magicnum);
    return -ENXIO;
  }
  /* dbg: check if already marked notified */
  if (CC_UNLIKELY(itr->is_notified(caller))) {
    MY_LOGW("recrod(m=%u,request_api=%d,caller=%#x) has been notified already.",
            itr->magicnum, itr->fd, caller);
  }

  /* mark notified */
  itr->mark_notified(caller);

  /* check if all caller has notified */
  if (itr->is_notified((SYNC_ID)getAllUsers())) {
    err = ioctl_media_request_queue_locked(itr->fd, itr->magicnum);
  }

  if (CC_UNLIKELY(err != 0)) {
    MY_LOGE("notifyEnqueuedByMagicNum failed, errcode=%#x", err);
  }

  return err;
}

int SyncReqMgr::notifyEnqueuedByRequestAPI(SYNC_ID caller, int fd) {
  int err = 0;

  std::lock_guard<std::mutex> lk(m_opLock);
  auto itr = std::find_if(m_records.begin(), m_records.end(),
                          [fd](const Record& el) { return el.fd == fd; });
  if (CC_UNLIKELY(itr == m_records.end())) {
    MY_LOGE(
        "caller(%#x) cannot found the RequestAPI fd(%d), maybe something wrong",
        caller, fd);
    return -ENXIO;
  }
  /* dbg: check if already marked notified */
  if (CC_UNLIKELY(itr->is_notified(caller))) {
    MY_LOGW("recrod(m=%u,request_api=%d,caller=%#x) has been notified already.",
            itr->magicnum, itr->fd, caller);
  }

  /* mark notified */
  itr->mark_notified(caller);

  /* check if all caller has notified */
  if (itr->is_notified((SYNC_ID)getAllUsers())) {
    err = ioctl_media_request_queue_locked(itr->fd, itr->magicnum);
  }

  if (CC_UNLIKELY(err != 0)) {
    MY_LOGE("notifyEnqueuedByRequestAPI failed, errcode=%#x", err);
  }

  return err;
}

void SyncReqMgr::reserve_requestapi_from_driver_locked(size_t count) {
  for (size_t i = 0; i < count; ++i) {
    int fd = request_fd_locked();
    if (CC_UNLIKELY(fd <= 0)) {
      MY_LOGE("request a FD by RequestAPI failed, got nullptr.(i=%zu)", i);
      break;
    }
    /* push back into m_records */
    Record r;
    r.fd = fd;
    r.magicnum = m_magicNum++;
    m_records.emplace_back(r);
  }

  MY_LOGD("m_records size=%zu", m_records.size());
}

std::vector<SyncReqMgr::Record>::iterator
SyncReqMgr::ask_requestapi_from_driver_locked() {
  int fd = request_fd_locked();
  if (CC_UNLIKELY(fd <= 0)) {
    MY_LOGE("request a FD by RequestAPI failed, got invalid fd");
    return m_records.end();
  }

  uint32_t r_magicnum = m_magicNum++;
  Record r;
  r.fd = fd;
  r.magicnum = r_magicnum;
  m_records.emplace_back(r);
  return m_records.end() - 1;
}

void SyncReqMgr::return_requestapi_to_driver_locked(int fd) {
  close(fd);  // close Request API just like closing a file descriptor
}

int SyncReqMgr::request_fd_locked() {
  std::shared_ptr<MtkCameraV4L2API> p_api = m_pV4L2Api.lock();
  if (CC_UNLIKELY(p_api.get() == nullptr)) {
    MY_LOGE(
        "cannot request a RequestAPI FD since no MtkCameraV4L2API instance");
    return -1;  // return illegal fd id for error return
  }

  int request_fd = 0;
  auto err = p_api->allocateRequest(m_mediadev_idx, &request_fd);
  if (CC_UNLIKELY(err != 0)) {
    MY_LOGE("allocateRequest returns error(code=%#x)", err);
    return -1;  // return illegal fd id for error return
  }

  MY_LOGD("allocateRequest request_api=%d", request_fd);
  return request_fd;
}

void SyncReqMgr::recycle_fd_locked(int fd) {
  std::shared_ptr<MtkCameraV4L2API> p_api = m_pV4L2Api.lock();
  if (CC_UNLIKELY(p_api.get() == nullptr)) {
    MY_LOGE(
        "cannot recycle a RequestAPI FD since no MtkCameraV4L2API instance");
    return;
  }

  auto err = p_api->reInitRequest(m_mediadev_idx, fd);
  if (CC_UNLIKELY(err != 0))
    MY_LOGE("reInitRequest returns error(code=%d)", fd);
}

int SyncReqMgr::ioctl_media_request_queue_locked(int fd, uint32_t magicnum) {
  std::shared_ptr<MtkCameraV4L2API> p_api = m_pV4L2Api.lock();
  if (CC_UNLIKELY(p_api.get() == nullptr)) {
    MY_LOGE(
        "cannot request a RequestAPI FD since no MtkCameraV4L2API instance");
    return -EFAULT;
  }
  auto err = p_api->queueRequest(m_mediadev_idx, fd);
  if (CC_UNLIKELY(err != 0)) {
    MY_LOGE("queueRequest (magicnum=%d, request_api=%d) failed, errcode=%#x",
            magicnum, fd, err);
    return err;
  }

  MY_LOGD("MEDIA_REQUEST_IOC_QUEUE: magicnum=%d, request_api=%d", magicnum, fd);
  return 0;
}

}  // namespace v4l2
}  // namespace NSCam
