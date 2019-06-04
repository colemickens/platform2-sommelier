/*
 * Copyright (C) 2019 Mediatek Corporation.
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

#define LOG_TAG "Mediatek3AServer"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <memory>
#include <string>

#include <IPCHal3a.h>
#include <camera/hal/mediatek/mtkcam/ipc/server/Mediatek3AServer.h>
#include "Hal3aIpcServerAdapter.h"
#include "IPCSWNR.h"
#include "IPCIspMgr.h"

namespace NS3Av3 {

Mediatek3AServer* Mediatek3AServer::mInstance = nullptr;

void Mediatek3AServer::init() {
  LOG1("@%s", __FUNCTION__);

  if (mInstance == nullptr)
    mInstance = new Mediatek3AServer;
}

void Mediatek3AServer::deInit() {
  LOG1("@%s", __FUNCTION__);

  if (mInstance)
    delete mInstance;
  mInstance = nullptr;
}

Mediatek3AServer::Mediatek3AServer() : mCallback(nullptr) {
  LOG1("@%s", __FUNCTION__);

  int ret = pthread_rwlock_init(&mRWLock, NULL);
  if (ret != 0)
    LOGE("%s: fail to init rwlock!", __FUNCTION__);

  for (int i = 0; i < IPC_GROUP_NUM; i++) {
    std::string name =
        std::string("MTK3AServer") + std::to_string(i) + std::string(" Thread");
    mThreads[i] = std::unique_ptr<base::Thread>(new base::Thread(name));
    mThreads[i]->Start();
  }

  mHandleSeed = 1;
}

Mediatek3AServer::~Mediatek3AServer() {
  LOGE("@%s", __FUNCTION__);

  /* unmap all remained buffers */
  for (const auto& n : mShmInfoMap) {
    munmap(n.second.addr, n.second.size);
    close(n.second.fd);
  }
  pthread_rwlock_destroy(&mRWLock);
}

int32_t Mediatek3AServer::initialize(
    const camera_algorithm_callback_ops_t* callback_ops) {
  LOG1("@%s, callback_ops:%p", __FUNCTION__, callback_ops);

  CheckError((!callback_ops), -EINVAL, "@%s, the callback_ops is nullptr",
             __FUNCTION__);

  mCallback = callback_ops;

  return 0;
}

int32_t Mediatek3AServer::registerBuffer(int buffer_fd) {
  LOG1("@%s, buffer_fd:%d", __FUNCTION__, buffer_fd);

  pthread_rwlock_rdlock(&mRWLock);
  if (mHandles.find(buffer_fd) != mHandles.end()) {
    LOGE("@%s, Buffer already registered", __FUNCTION__);
    pthread_rwlock_unlock(&mRWLock);
    return -EINVAL;
  }
  pthread_rwlock_unlock(&mRWLock);

  struct stat sb;
  int ret = fstat(buffer_fd, &sb);
  CheckError((ret == -1), -EBADFD, "@%s, Failed to get buffer status",
             __FUNCTION__);

  /* query size for dma buf fd */
  uint64_t end_pos = lseek(buffer_fd, 0, SEEK_END);

  uint64_t mmap_size = (sb.st_size != 0 ? sb.st_size : end_pos);

  void* addr =
      mmap(0, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer_fd, 0);

  CheckError((addr == reinterpret_cast<void*>(-1)), -EBADFD,
             "@%s, Failed to map buffer", __FUNCTION__);

  pthread_rwlock_wrlock(&mRWLock);

  int32_t handle = mHandleSeed++;

  mHandles[buffer_fd] = handle;

  mShmInfoMap[handle].fd = buffer_fd;
  mShmInfoMap[handle].addr = addr;
  mShmInfoMap[handle].size = mmap_size;

  pthread_rwlock_unlock(&mRWLock);

  return handle;
}

int Mediatek3AServer::parseReqHeader(const uint8_t req_header[],
                                     uint32_t size) {
  if (size < IPC_REQUEST_HEADER_USED_NUM || req_header[0] != IPC_MATCHING_KEY) {
    IPC_LOGE("@%s, fail, request header size:%d, req_header[0]:%d",
             __FUNCTION__, size, req_header[0]);
    return -1;
  }

  LOG1("@%s, size:%d, req_header[1]:%d", __FUNCTION__, size, req_header[1]);

  return req_header[1];  // mSensorOpenIndex
}

void Mediatek3AServer::handleRequest(MsgReq msg) {
  uint32_t req_id = msg.req_id;
  int32_t buffer_handle = msg.buffer_handle;
  int reqeustSize = 0;
  void* addr = nullptr;

  pthread_rwlock_rdlock(&mRWLock);
  if (buffer_handle != -1) {
    if (mShmInfoMap.find(buffer_handle) == mShmInfoMap.end()) {
      IPC_LOGE("@%s, Invalid buffer handle", __FUNCTION__);
      pthread_rwlock_unlock(&mRWLock);
      returnCallback(req_id, UNKNOWN_ERROR, buffer_handle);
      return;
    }
    ShmInfo info = mShmInfoMap[buffer_handle];
    LOG1("@%s, info.fd:%d, info.size:%zu", __FUNCTION__, info.fd, info.size);

    reqeustSize = info.size;
    addr = info.addr;
  }
  pthread_rwlock_unlock(&mRWLock);

  LOG1("@%s, req_id:%d, reqeustSize:%d, addr:%p, buffer_handle:%d",
       __FUNCTION__, req_id, reqeustSize, addr, buffer_handle);

  if (addr == nullptr) {
    IPC_LOGE("@%s, addr is nullptr", __FUNCTION__);
    returnCallback(req_id, UNKNOWN_ERROR, buffer_handle);
    return;
  }

  status_t status = OK;
  switch (req_id) {
    case IPC_HAL3A_INIT:
      status = adapter_3a.init(addr, reqeustSize);
      break;
    case IPC_HAL3A_DEINIT:
      status = adapter_3a.uninit(addr, reqeustSize);
      break;
    case IPC_HAL3A_CONFIG:
      status = adapter_3a.config(addr, reqeustSize);
      break;
    case IPC_HAL3A_START:
      status = adapter_3a.start(addr, reqeustSize);
      break;
    case IPC_HAL3A_STOP:
      status = adapter_3a.stop(addr, reqeustSize);
      break;
    case IPC_HAL3A_STOP_STT:
      status = adapter_3a.stopStt(addr, reqeustSize);
      break;
    case IPC_HAL3A_SET:
      status = adapter_3a.set(addr, reqeustSize);
      break;
    case IPC_HAL3A_SETISP: {
      hal3a_setisp_params* params = static_cast<hal3a_setisp_params*>(addr);
      pthread_rwlock_rdlock(&mRWLock);
      if (params->u4LceEnable == 1)
        params->lceBufInfo.bufVA[0] = reinterpret_cast<MUINTPTR>(
            mShmInfoMap[params->lceBufInfo.fd[0]].addr);
      params->p2tuningbuf_va = reinterpret_cast<MUINTPTR>(
          mShmInfoMap[params->p2tuningbuf_handle].addr);
      pthread_rwlock_unlock(&mRWLock);
      status = adapter_3a.setIsp(addr, reqeustSize);
    } break;
    case IPC_HAL3A_PRESET:
      status = adapter_3a.preset(addr, reqeustSize);
      break;
    case IPC_HAL3A_SEND3ACTRL:
      status = adapter_3a.send3ACtrl(addr, reqeustSize);
      break;
    case IPC_HAL3A_GETSENSORPARAM:
    case IPC_HAL3A_GETSENSORPARAM_ENABLE:
      status = adapter_3a.getSensorParam(addr, reqeustSize);
      break;
    case IPC_HAL3A_NOTIFYCB:
    case IPC_HAL3A_NOTIFYCB_ENABLE:
      status = adapter_3a.notifyCallBack(addr, reqeustSize);
      break;
    case IPC_HAL3A_TUNINGPIPE:
    case IPC_HAL3A_TUNINGPIPE_TERM: {
      hal3a_tuningpipe_params* params =
          static_cast<hal3a_tuningpipe_params*>(addr);
      pthread_rwlock_rdlock(&mRWLock);
      params->p1tuningbuf_va = reinterpret_cast<MUINTPTR>(
          mShmInfoMap[params->p1tuningbuf_handle].addr);
      pthread_rwlock_unlock(&mRWLock);
      status = adapter_3a.tuningPipe(addr, reqeustSize);
      break;
    }
    case IPC_HAL3A_STTPIPE: {
      hal3a_sttpipe_params* params = static_cast<hal3a_sttpipe_params*>(addr);
      pthread_rwlock_rdlock(&mRWLock);
      if (params->e3ACtrl == E3ACtrl_IPC_P1_SttControl &&
          IPC_Metabuf1_T::cmdENQUE_FROM_DRV == params->arg1.ipcMetaBuf.cmd)
        params->arg1.ipcMetaBuf.bufVa = reinterpret_cast<uint64_t>(
            mShmInfoMap[params->arg1.ipcMetaBuf.bufFd].addr);
      pthread_rwlock_unlock(&mRWLock);
      status = adapter_3a.sttPipe(addr, reqeustSize);

      if (params->e3ACtrl == E3ACtrl_IPC_P1_SttControl &&
          IPC_Metabuf1_T::cmdDEQUE_FROM_3A == params->arg1.ipcMetaBuf.cmd &&
          params->arg1.ipcMetaBuf.response == IPC_Metabuf1_T::responseOK) {
        pthread_rwlock_rdlock(&mRWLock);
        for (const auto& n : mShmInfoMap) {
          if (reinterpret_cast<uint64_t>(n.second.addr) ==
              params->arg1.ipcMetaBuf.bufVa) {
            params->arg1.ipcMetaBuf.bufFd = n.first;
            break;
          }
        }
        pthread_rwlock_unlock(&mRWLock);
      }
    } break;
    case IPC_HAL3A_STT2PIPE: {
      hal3a_stt2pipe_params* params = static_cast<hal3a_stt2pipe_params*>(addr);
      pthread_rwlock_rdlock(&mRWLock);
      if (params->e3ACtrl == E3ACtrl_IPC_P1_Stt2Control &&
          IPC_Metabuf2_T::cmdENQUE_FROM_DRV == params->arg1.ipcMetaBuf2.cmd)
        params->arg1.ipcMetaBuf2.bufVa = reinterpret_cast<uint64_t>(
            mShmInfoMap[params->arg1.ipcMetaBuf2.bufFd].addr);
      pthread_rwlock_unlock(&mRWLock);
      status = adapter_3a.stt2Pipe(addr, reqeustSize);

      if (params->e3ACtrl == E3ACtrl_IPC_P1_Stt2Control &&
          IPC_Metabuf2_T::cmdDEQUE_FROM_3A == params->arg1.ipcMetaBuf2.cmd &&
          params->arg1.ipcMetaBuf2.response == IPC_Metabuf2_T::responseOK) {
        pthread_rwlock_rdlock(&mRWLock);
        for (const auto& n : mShmInfoMap) {
          if (reinterpret_cast<uint64_t>(n.second.addr) ==
              params->arg1.ipcMetaBuf2.bufVa) {
            params->arg1.ipcMetaBuf2.bufFd = n.first;
            break;
          }
        }
        pthread_rwlock_unlock(&mRWLock);
      }
    } break;
    case IPC_HAL3A_HWEVENT:
      status = adapter_3a.hwEvent(addr, reqeustSize);
      break;
    case IPC_HAL3A_AEPLINELIMIT:
      status = adapter_3a.aePlineLimit(addr, reqeustSize);
      break;
    case IPC_HAL3A_NOTIFY_P1_PWR_ON:
      status = adapter_3a.notifyP1PwrOn(addr, reqeustSize);
      break;
    case IPC_HAL3A_NOTIFY_P1_PWR_DONE:
      status = adapter_3a.notifyP1Done(addr, reqeustSize);
      break;
    case IPC_HAL3A_NOTIFY_P1_PWR_OFF:
      status = adapter_3a.notifyP1PwrOff(addr, reqeustSize);
      break;
    case IPC_HAL3A_SET_SENSOR_MODE:
      status = adapter_3a.setSensorMode(addr, reqeustSize);
      break;
    case IPC_HAL3A_ATTACH_CB:
      status = adapter_3a.attachCb(addr, reqeustSize);
      break;
    case IPC_HAL3A_DETACH_CB:
      status = adapter_3a.detachCb(addr, reqeustSize);
      break;
    case IPC_HAL3A_GET:
      status = adapter_3a.get(addr, reqeustSize);
      break;
    case IPC_HAL3A_GET_CUR:
      status = adapter_3a.getCur(addr, reqeustSize);
      break;
    case IPC_HAL3A_SET_FDINFO:
      status = adapter_3a.setFDInfoOnActiveArray(addr, reqeustSize);
      break;
    case IPC_SWNR_CREATE:
      status = adapter_swnr.create(addr, reqeustSize);
      break;
    case IPC_SWNR_DESTROY:
      status = adapter_swnr.destroy(addr, reqeustSize);
      break;
    case IPC_SWNR_DO_SWNR: {
      IPCSWNR::DoSwNrParams* params = static_cast<IPCSWNR::DoSwNrParams*>(addr);
      pthread_rwlock_rdlock(&mRWLock);
      params->imagebuf_info.va = reinterpret_cast<MUINTPTR>(
          mShmInfoMap[params->imagebuf_info.buf_handle].addr);
      pthread_rwlock_unlock(&mRWLock);
      status = adapter_swnr.doSwNR(addr, reqeustSize);
      break;
    }
    case IPC_SWNR_GET_DEBUGINFO:
      status = adapter_swnr.getDebugInfo(addr, reqeustSize);
      break;
    case IPC_LCS_CREATE:
      status = adapter_lcs.create(addr, reqeustSize);
      break;
    case IPC_LCS_INIT:
      status = adapter_lcs.init(addr, reqeustSize);
      break;
    case IPC_LCS_CONFIG:
      status = adapter_lcs.config(addr, reqeustSize);
      break;
    case IPC_LCS_UNINIT:
      status = adapter_lcs.uninit(addr, reqeustSize);
      break;
    case IPC_ISPMGR_CREATE:
      status = adapter_ispmgr.create(addr, reqeustSize);
      break;
    case IPC_ISPMGR_QUERYLCSO:
      status = adapter_ispmgr.querylcso(addr, reqeustSize);
      break;
    case IPC_ISPMGR_PPNR3D: {
      ispmgr_ppnr3d_params* params = static_cast<ispmgr_ppnr3d_params*>(addr);
      pthread_rwlock_rdlock(&mRWLock);
      params->p2tuningbuf_va = reinterpret_cast<MUINTPTR>(
          mShmInfoMap[params->p2tuningbuf_handle].addr);
      pthread_rwlock_unlock(&mRWLock);
      status = adapter_ispmgr.ppnr3d(addr, reqeustSize);
      break;
    }
    case IPC_NR3D_EIS_CREATE:
      status = adapter_nr3d.create(addr, reqeustSize);
      break;
    case IPC_NR3D_EIS_DESTROY:
      status = adapter_nr3d.destroy(addr, reqeustSize);
      break;
    case IPC_NR3D_EIS_INIT:
      status = adapter_nr3d.init(addr, reqeustSize);
      break;
    case IPC_NR3D_EIS_MAIN:
      status = adapter_nr3d.main(addr, reqeustSize);
      break;
    case IPC_NR3D_EIS_RESET:
      status = adapter_nr3d.reset(addr, reqeustSize);
      break;
    case IPC_NR3D_EIS_FEATURECTRL:
      status = adapter_nr3d.featureCtrl(addr, reqeustSize);
      break;
    case IPC_FD_CREATE:
      status = adapter_fd.create(addr, reqeustSize);
      break;
    case IPC_FD_DESTORY:
      status = adapter_fd.destory(addr, reqeustSize);
      break;
    case IPC_FD_INIT:
      status = adapter_fd.init(addr, reqeustSize);
      break;
    case IPC_FD_MAIN: {
      FDMainParam* Params = static_cast<FDMainParam*>(addr);
      pthread_rwlock_rdlock(&mRWLock);
      (Params->common).bufferva =
          reinterpret_cast<MUINTPTR>(mShmInfoMap[Params->fdBuffer].addr);
      pthread_rwlock_unlock(&mRWLock);
      status = adapter_fd.main(addr, reqeustSize);
    } break;
    case IPC_FD_GET_CAL_DATA:
      status = adapter_fd.getCalData(addr, reqeustSize);
      break;
    case IPC_FD_SET_CAL_DATA:
      status = adapter_fd.setCalData(addr, reqeustSize);
      break;
    case IPC_FD_MAIN_PHASE2:
      status = adapter_fd.mainPhase2(addr, reqeustSize);
      break;
    case IPC_FD_GETRESULT:
      status = adapter_fd.getResult(addr, reqeustSize);
      break;
    case IPC_FD_RESET:
      status = adapter_fd.reset(addr, reqeustSize);
      break;
    case IPC_HAL3A_AFLENSCONFIG:
    case IPC_HAL3A_AFLENS_ENABLE:
      status = adapter_3a.afLensConfig(addr, reqeustSize);
      break;
    default:
      IPC_LOGE("@%s, req_id:%d is not defined", __FUNCTION__, req_id);
      status = UNKNOWN_ERROR;
      break;
  }

  returnCallback(req_id, status, buffer_handle);
}

void Mediatek3AServer::request(uint32_t req_id,
                               const uint8_t req_header[],
                               uint32_t size,
                               int32_t buffer_handle) {
  LOG1("@%s, size:%d, buffer_handle:%d", __FUNCTION__, size, buffer_handle);

  int servGroup = parseReqHeader(req_header, size);
  if (servGroup == -1) {
    returnCallback(req_id, UNKNOWN_ERROR, buffer_handle);
    return;
  }

  MsgReq msg = {req_id, buffer_handle};
  mThreads[servGroup]->task_runner()->PostTask(
      FROM_HERE, base::Bind(&Mediatek3AServer::handleRequest,
                            base::Unretained(this), msg));
}

void Mediatek3AServer::notify(uint32_t req_id, uint32_t rc) {
  returnCallback(req_id, OK, rc);
}

void Mediatek3AServer::deregisterBuffers(const int32_t buffer_handles[],
                                         uint32_t size) {
  LOG1("@%s, size:%d", __FUNCTION__, size);

  pthread_rwlock_wrlock(&mRWLock);

  for (uint32_t i = 0; i < size; i++) {
    int32_t handle = buffer_handles[i];
    if (mShmInfoMap.find(handle) == mShmInfoMap.end()) {
      continue;
    }

    mHandles.erase(mShmInfoMap[handle].fd);

    munmap(mShmInfoMap[handle].addr, mShmInfoMap[handle].size);
    close(mShmInfoMap[handle].fd);
    mShmInfoMap.erase(handle);
  }
  pthread_rwlock_unlock(&mRWLock);
}

void Mediatek3AServer::returnCallback(uint32_t req_id,
                                      status_t status,
                                      int32_t buffer_handle) {
  LOG1("@%s, buffer_handle:%d", __FUNCTION__, buffer_handle);
  (*mCallback->return_callback)(mCallback, req_id, status, buffer_handle);
}

static int32_t initialize(const camera_algorithm_callback_ops_t* callback_ops) {
  LOG1("@%s, callback_ops:%p", __FUNCTION__, callback_ops);
  return Mediatek3AServer::getInstance()->initialize(callback_ops);
}

static int32_t registerBuffer(int32_t buffer_fd) {
  LOG1("@%s, buffer_fd:%d", __FUNCTION__, buffer_fd);
  return Mediatek3AServer::getInstance()->registerBuffer(buffer_fd);
}

static void request(uint32_t req_id,
                    const uint8_t req_header[],
                    uint32_t size,
                    int32_t buffer_handle) {
  LOG1("@%s, size:%d, buffer_handle:%d", __FUNCTION__, size, buffer_handle);
  Mediatek3AServer::getInstance()->request(req_id, req_header, size,
                                           buffer_handle);
}

static void deregisterBuffers(const int32_t buffer_handles[], uint32_t size) {
  LOG1("@%s, size:%d", __FUNCTION__, size);
  return Mediatek3AServer::getInstance()->deregisterBuffers(buffer_handles,
                                                            size);
}

extern "C" {
camera_algorithm_ops_t CAMERA_ALGORITHM_MODULE_INFO_SYM
    __attribute__((__visibility__("default"))) = {
        .initialize = initialize,
        .register_buffer = registerBuffer,
        .request = request,
        .deregister_buffers = deregisterBuffers};
}

__attribute__((constructor)) void initMediatek3AServer() {
  Mediatek3AServer::init();
}

__attribute__((destructor)) void deinitMediatek3AServer() {
  Mediatek3AServer::deInit();
}

}  // namespace NS3Av3
