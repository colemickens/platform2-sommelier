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

#include <cutils/compiler.h>
#include <memory>
#include <stdio.h>

#include <camera/hal/mediatek/mtkcam/ipc/server/SWNRIpcServerAdapter.h>

#include "mtkcam/utils/std/Log.h"
#include "IPCSWNR.h"
#include "Errors.h"

#undef LOG_TAG
#define LOG_TAG "SWNR_IPC_SERVER"

namespace IPCSWNR {

int SWNRIpcServerAdapter::extractSensor(void* addr) {
  int s_idx;
  CommonParams* params = static_cast<CommonParams*>(addr);
  s_idx = params->sensor_idx;
  if (CC_UNLIKELY(s_idx >= IPC_MAX_SENSOR_NUM || s_idx < 0)) {
    MY_LOGE("sensor index %d is illegal, should less than %d and is postive",
            s_idx, IPC_MAX_SENSOR_NUM);
    return -1;
  }
  return s_idx;
}

int SWNRIpcServerAdapter::create(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  CheckError((dataSize < sizeof(CreateParams)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  int sensor_idx = 0;
  sensor_idx = extractSensor(addr);
  if (sensor_idx < 0)
    return -1;

  std::lock_guard<std::mutex> _lk(create_lock);
  if (!msp_swnr[sensor_idx]) {
    MY_LOGD("MAKE_SWNR, sensor index is %d", sensor_idx);
    msp_swnr[sensor_idx] = MAKE_SwNR(sensor_idx);
  }
  TRACE_FUNC_EXIT();
  return OK;
}

int SWNRIpcServerAdapter::destroy(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  CheckError((dataSize < sizeof(DestroyParams)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  int sensor_idx = 0;
  sensor_idx = extractSensor(addr);
  if (sensor_idx < 0)
    return -1;

  std::lock_guard<std::mutex> _lk(create_lock);
  if (msp_swnr[sensor_idx]) {
    MY_LOGD("delete SWNR, sensor index is %d", sensor_idx);
    delete msp_swnr[sensor_idx];
    msp_swnr[sensor_idx] = nullptr;
  }
  TRACE_FUNC_EXIT();
  return OK;
}

int SWNRIpcServerAdapter::doSwNR(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  CheckError((dataSize < sizeof(DoSwNrParams)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  int sensor_idx = 0;
  sensor_idx = extractSensor(addr);
  if (sensor_idx < 0)
    return -1;

  if (CC_UNLIKELY(!msp_swnr[sensor_idx])) {
    MY_LOGE(
        "swnr pointer at sensor index %d is null, should be created firstly",
        sensor_idx);
    return INVALID_OPERATION;
  }

  DoSwNrParams* params = static_cast<DoSwNrParams*>(addr);
  if (!params) {
    MY_LOGE("Do SWNR Buffer is NULL");
    return -1;
  }

  IPCImageBufAllocator::config cfg{};

  cfg.format = params->imagebuf_info.format;
  cfg.width = params->imagebuf_info.width;
  cfg.height = params->imagebuf_info.height;
  cfg.planecount = params->imagebuf_info.plane_cnt;

  for (int i = 0; i < cfg.planecount; i++) {
    cfg.strides[i] = params->imagebuf_info.strides_bytes[i];
    cfg.stridepixel[i] = params->imagebuf_info.strides_pixel[i];
    cfg.scanlines[i] = params->imagebuf_info.scanlines[i];
    cfg.bufsize[i] = params->imagebuf_info.buf_size[i];
    cfg.fd[i] = params->imagebuf_info.buf_handle;
    /* caculate plane1 and plane2 buf va by buffer size offset */
    cfg.va[i] =
        (i == 0 ? params->imagebuf_info.va
                : cfg.va[i - 1] + params->imagebuf_info.buf_size[i - 1]);
  }

  IPCImageBufAllocator allocator(cfg, "IPC_SWNR");

  /* allocate an IImageBuffer */
  std::shared_ptr<NSCam::IImageBuffer> imgbuf = allocator.createImageBuffer();

  imgbuf->lockBuf("IPC_SWNR");
  if (!msp_swnr[sensor_idx]->doSwNR(params->swnr_param, imgbuf.get()))
    return INVALID_OPERATION;
  imgbuf->unlockBuf("IPC_SWNR");

  TRACE_FUNC_EXIT();
  return OK;
}

int SWNRIpcServerAdapter::getDebugInfo(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  CheckError((dataSize < sizeof(GetDebugInfoParams)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  int sensor_idx = 0;
  sensor_idx = extractSensor(addr);
  if (sensor_idx < 0)
    return -1;

  if (CC_UNLIKELY(!msp_swnr[sensor_idx])) {
    MY_LOGE(
        "swnr pointer at sensor index %d is null, should be created firstly",
        sensor_idx);
    return INVALID_OPERATION;
  }

  GetDebugInfoParams* params = static_cast<GetDebugInfoParams*>(addr);
  if (!params) {
    MY_LOGE("SWNR Get Debug Info Buffer is NULL");
    return -1;
  }

  /* get metadata from client */
  NSCam::IMetadata hal_metadata;
  ssize_t inHalSize = hal_metadata.unflatten(&params->hal_metadata,
                                             sizeof(params->hal_metadata));
  if (inHalSize < 0) {
    MY_LOGE("GetDebugInfo: Hal Metadata unflatten failed");
    return -1;
  }

  /* send to swnr algo */
  if (!msp_swnr[sensor_idx]->getDebugInfo(&hal_metadata))
    return BAD_VALUE;

  /* send metadata back to client */
  ssize_t outHalSize =
      hal_metadata.flatten(&params->hal_metadata, sizeof(params->hal_metadata));
  if (outHalSize < 0) {
    MY_LOGE("GetDebugInfo: Hal Metadata flatten failed");
    return -1;
  }
  TRACE_FUNC_EXIT();
  return OK;
}

}  // namespace IPCSWNR
