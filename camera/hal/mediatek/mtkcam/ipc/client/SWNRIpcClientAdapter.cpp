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

#include <camera/hal/mediatek/mtkcam/ipc/client/SWNRIpcClientAdapter.h>

#include "mtkcam/utils/std/Log.h"
#include "Hal3aIpcCommon.h"
#include "IPCSWNR.h"

#undef LOG_TAG
#define LOG_TAG "SWNR_IPC_CLIENT"

namespace IPCSWNR {

VISIBILITY_PUBLIC extern "C" ISwNR* createInstance_SWNR_Client(
    const MINT32 sensor_idx) {
  TRACE_FUNC_ENTER();

  if (CC_UNLIKELY(sensor_idx >= IPC_MAX_SENSOR_NUM)) {
    MY_LOGE("sensor index %d is illegal, should less than %d", sensor_idx,
            IPC_MAX_SENSOR_NUM);
    return nullptr;
  }

  SWNRIpcClientAdapter* swnr_adapter = new SWNRIpcClientAdapter(sensor_idx);

  if (CC_UNLIKELY(!swnr_adapter->m_initialized)) {
    delete swnr_adapter;
    swnr_adapter = nullptr;
  }

  TRACE_FUNC_EXIT();
  return static_cast<ISwNR*>(swnr_adapter);
}

SWNRIpcClientAdapter::SWNRIpcClientAdapter(const MINT32 sensor_idx)
    : m_initialized(false), m_sensor_idx(sensor_idx) {
  mv_mems = {
      {"/mtkSWNR_Create", sizeof(CreateParams), &m_meminfo_create, false},
      {"/mtkSWNR_Destroy", sizeof(DestroyParams), &m_meminfo_destroy, false},
      {"/mtkSWNR_DoSWNR", sizeof(DoSwNrParams), &m_meminfo_do_swnr, false},
      {"/mtkSWNR_GetDebugInfo", sizeof(GetDebugInfoParams),
       &m_meminfo_get_debuginfo, false}};
  TRACE_FUNC_ENTER();

  int ret = false;
  m_ipc_common.init(m_sensor_idx);
  ret = m_ipc_common.allocateAllShmMems(&mv_mems);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("construction fail while allocate ipc shared memroy");
    m_ipc_common.releaseAllShmMems(&mv_mems);
    return;
  }

  ret = sendRequest(IPC_SWNR_CREATE, &m_meminfo_create);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("construction fail while create");
    m_ipc_common.releaseAllShmMems(&mv_mems);
    return;
  }

  m_initialized = true;

  TRACE_FUNC_EXIT();
}

SWNRIpcClientAdapter::~SWNRIpcClientAdapter() {
  TRACE_FUNC_ENTER();

  if (CC_LIKELY(m_initialized)) {
    int ret = sendRequest(IPC_SWNR_DESTROY, &m_meminfo_destroy);
    if (CC_UNLIKELY(!ret))
      MY_LOGE("destroy fail, memory leak will happen");
  }

  m_ipc_common.releaseAllShmMems(&mv_mems);
  TRACE_FUNC_EXIT();
}

MBOOL SWNRIpcClientAdapter::doSwNR(const SWNRParam& swnr_param,
                                   NSCam::IImageBuffer* p_buf) {
  TRACE_FUNC_ENTER();
  DoSwNrParams* params = static_cast<DoSwNrParams*>(m_meminfo_do_swnr.mAddr);
  if (!p_buf) {
    MY_LOGE("doSwNR : Image Buffer is NULL");
    return false;
  }
  int buf_fd = p_buf->getFD(0);
  int buf_handle = m_ipc_common.registerBuffer(buf_fd);

  /* we leverage this buffer info to re-construct IImage buffer at IPC server */
  params->swnr_param = swnr_param;
  params->imagebuf_info.format = p_buf->getImgFormat();
  params->imagebuf_info.width = p_buf->getImgSize().w;
  params->imagebuf_info.height = p_buf->getImgSize().h;
  params->imagebuf_info.buf_handle = buf_handle;
  params->imagebuf_info.plane_cnt = p_buf->getPlaneCount();

  for (int i = 0; i < params->imagebuf_info.plane_cnt; i++) {
    params->imagebuf_info.strides_bytes[i] = p_buf->getBufStridesInBytes(i);
    params->imagebuf_info.strides_pixel[i] = p_buf->getBufStridesInPixel(i);
    params->imagebuf_info.buf_size[i] = p_buf->getBufSizeInBytes(i);
    params->imagebuf_info.scanlines[i] = p_buf->getBufScanlines(i);
  }

  int ret = sendRequest(IPC_SWNR_DO_SWNR, &m_meminfo_do_swnr);
  m_ipc_common.deregisterBuffer(buf_handle);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("sync request fail with error status %d", ret);
    return false;
  }

  TRACE_FUNC_EXIT();
  return true;
}

MBOOL SWNRIpcClientAdapter::getDebugInfo(NSCam::IMetadata* hal_metadata) {
  TRACE_FUNC_ENTER();
  GetDebugInfoParams* params =
      static_cast<GetDebugInfoParams*>(m_meminfo_get_debuginfo.mAddr);

  size_t input_metadata_size = hal_metadata->flatten(
      &params->hal_metadata, sizeof(params->hal_metadata));
  if (input_metadata_size < 0) {
    MY_LOGE("SWNR : getDebugInfo : flatten hal metadata failed");
    return false;
  }
  MY_LOGD("client input_hal_metadata size is %zu", input_metadata_size);

  int ret = sendRequest(IPC_SWNR_GET_DEBUGINFO, &m_meminfo_get_debuginfo);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("sync request fail with error status %d", ret);
    return false;
  }

  size_t result_metadata_size = hal_metadata->unflatten(
      &params->hal_metadata, sizeof(params->hal_metadata));
  if (result_metadata_size < 0) {
    MY_LOGE("SWNR : getDebugInfo : unflatten hal metadata failed");
    return false;
  }
  MY_LOGD("client result_hal_metadata size is %zu", result_metadata_size);
  TRACE_FUNC_EXIT();
  return true;
}

MBOOL SWNRIpcClientAdapter::sendRequest(IPC_CMD cmd,
                                        ShmMemInfo* meminfo,
                                        int group) {
  CommonParams* params = static_cast<CommonParams*>(meminfo->mAddr);
  params->sensor_idx = m_sensor_idx;

  return m_ipc_common.requestSync(cmd, meminfo->mHandle, group);
}

}  // namespace IPCSWNR
