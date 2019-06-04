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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_HAL3AIPCSERVERADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_HAL3AIPCSERVERADAPTER_H_

#include <IHal3A.h>
#include <IHal3ACb.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <memory>
#include <vector>

namespace NS3Av3 {

class Hal3aIpcServerAdapter : public IHal3ACb {
 public:
  int init(void* addr, int dataSize);
  int uninit(void* addr, int dataSize);
  int config(void* addr, int dataSize);
  int start(void* addr, int dataSize);
  int stop(void* addr, int dataSize);
  int stopStt(void* addr, int dataSize);
  int set(void* addr, int dataSize);
  int setIsp(void* addr, int dataSize);
  int startRequestQ(void* addr, int dataSize);
  int startCapture(void* addr, int dataSize);
  int preset(void* addr, int dataSize);
  int send3ACtrl(void* addr, int dataSize);
  int getSensorParam(void* addr, int dataSize);
  int notifyCallBack(void* addr, int dataSize);
  int tuningPipe(void* addr, int dataSize);
  int sttPipe(void* addr, int dataSize);
  int stt2Pipe(void* addr, int dataSize);
  int hwEvent(void* addr, int dataSize);
  int aePlineLimit(void* addr, int dataSize);
  int afLensConfig(void* addr, int dataSize);
  int notifyP1PwrOn(void* addr, int dataSize);
  int notifyP1Done(void* addr, int dataSize);
  int notifyP1PwrOff(void* addr, int dataSize);
  int setSensorMode(void* addr, int dataSize);
  int attachCb(void* addr, int dataSize);
  int detachCb(void* addr, int dataSize);
  int get(void* addr, int dataSize);
  int getCur(void* addr, int dataSize);
  bool setFDInfoOnActiveArray(void* addr, int dataSize);

  Hal3aIpcServerAdapter() {
    std::fill_n(addrMapping, IHal3ACb::eID_MSGTYPE_NUM, nullptr);
  }
  virtual ~Hal3aIpcServerAdapter() {}
  virtual void doNotifyCb(MINT32 _msgType,
                          MINTPTR _ext1,
                          MINTPTR _ext2,
                          MINTPTR _ext3);

 private:
  int hal3a_server_parsing_sensor_idx(void* addr);

  int hal3a_server_metaset_unflatten(struct hal3a_metaset_params* params,
                                     MetaSet_T* metaSet,
                                     std::vector<MetaSet_T*>* requestQ);

  std::shared_ptr<NS3Av3::IHal3A> mpHal3A[IPC_MAX_SENSOR_NUM];

  void* addrMapping[IHal3ACb::eID_MSGTYPE_NUM];

  std::shared_ptr<NSCam::IImageBuffer> m_pLceImgBuf;
};

}  // namespace NS3Av3

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_HAL3AIPCSERVERADAPTER_H_
