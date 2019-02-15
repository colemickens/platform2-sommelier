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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1COMMON_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1COMMON_H_
//
#include <sys/prctl.h>
#include <sys/resource.h>
#include <atomic>
#include <list>
#include <queue>
#include <string>
#include <thread>
#include <vector>
//
#include "P1Config.h"  // buildup configuration
//
#include <mtkcam/def/PriorityDefs.h>
//
#include <ImageDesc.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/std/DebugScanLine.h>
//
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
//
#include <mtkcam/utils/metadata/mtk_metadata_types.h>
//
#include <mtkcam/utils/metastore/IMetadataProvider.h>
//
#include <mtkcam/utils/imgbuf/IGbmImageBufferHeap.h>
//
#if (P1NODE_USING_DRV_IO_PIPE_EVENT > 0)
#include <mtkcam/drv/iopipe/Event/IoPipeEvent.h>
#endif
#include <mtkcam/drv/iopipe/CamIO/Cam_Notify.h>
#include <mtkcam/drv/iopipe/CamIO/Cam_QueryDef.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IHalCamIO.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IIOPipe.h>
#if (P1NODE_SUPPORT_RSS > 0)
#include <mtkcam/drv/iopipe/CamIO/rss_cb.h>
#endif
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/aaa/lcs/lcs_hal.h>
//
//
#include <mtkcam/pipeline/hwnode/P1Node.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/utils/streambuf/IStreamBufferPool.h>
//
#include <mtkcam/utils/hw/GyroCollector.h>
#include <mtkcam/utils/hw/HwInfoHelper.h>
#include <mtkcam/utils/hw/IResourceConcurrency.h>
#if (P1NODE_SUPPORT_BUFFER_TUNING_DUMP > 0)
#include <mtkcam/utils/TuningUtils/FileDumpNamingRule.h>
#endif
//
#include "../BaseNode.h"
#include "../MyUtils.h"
#include "../Profile.h"
#include <compiler.h>
//

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
namespace NSCam {
namespace v3 {
namespace NSP1Node {

/******************************************************************************
 *
 ******************************************************************************/
#define SUPPORT_3A (P1NODE_SUPPORT_3A)
#define SUPPORT_ISP (P1NODE_SUPPORT_ISP)
#define SUPPORT_PERFRAME_CTRL (P1NODE_SUPPORT_PERFRAME_CTRL)
//
#define SUPPORT_LCS (P1NODE_SUPPORT_LCS)
#define SUPPORT_RSS (P1NODE_SUPPORT_RSS)
#define SUPPORT_FSC (P1NODE_SUPPORT_FSC)

//
#define SUPPORT_RRZ_DST_CTRL (P1NODE_SUPPORT_RRZ_DST_CTRL)

//
#define SUPPORT_CONFIRM_BUF_PA (P1NODE_SUPPORT_CONFIRM_BUF_PA)
#define SUPPORT_CONFIRM_BUF_PA_VA (P1NODE_SUPPORT_CONFIRM_BUF_PA_VA)

//
#define SUPPORT_BUFFER_TUNING_DUMP (P1NODE_SUPPORT_BUFFER_TUNING_DUMP)

//
#define USING_CTRL_3A_LIST (P1NODE_USING_CTRL_3A_LIST)
#define USING_CTRL_3A_LIST_PREVIOUS \
  (P1NODE_USING_CTRL_3A_LIST_PREVIOUS)  // USING_PREVIOUS_3A_LIST

//
#define USING_DRV_SET_RRZ_CBFP_EXP_SKIP (P1NODE_USING_DRV_SET_RRZ_CBFP_EXP_SKIP)
#define USING_DRV_QUERY_CAPABILITY_EXP_SKIP \
  (P1NODE_USING_DRV_QUERY_CAPABILITY_EXP_SKIP)

//
#define USING_DRV_IO_PIPE_EVENT (P1NODE_USING_DRV_IO_PIPE_EVENT)

//
#define ENABLE_CHECK_CONFIG_COMMON_PORPERTY \
  (P1NODE_ENABLE_CHECK_CONFIG_COMMON_PORPERTY)

/******************************************************************************
 *
 ******************************************************************************/
#define IS_P1_LOGI \
  (MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT >= 2)  // for system use LOGI
#define IS_P1_LOGD \
  (MTKCAM_HW_NODE_LOG_LEVEL_DEFAULT >= 3)  // for system use LOGD
#define P1_LOG_LEN (256)

#define P1_SUPPORT_DIR_RESTREAM (1)
#define P1_LOG_NOTE_TAG "[::P1_LOG_NOTE::]"
#define P1_LOG_DUMP_TAG "[::P1_LOG_DUMP::]"
#define P1_LOG_LINE_BGN                  \
  " ++++++++ ++++++++ ++++++++ ++++++++" \
  " ++++++++ ++++++++ ++++++++ ++++++++" \
  " ++++++++ ++++++++ ++++++++ ++++++++" \
  " ++++++++ ++++++++ ++++++++ ++++++++"
#define P1_LOG_LINE_END                  \
  " -------- -------- -------- --------" \
  " -------- -------- -------- --------" \
  " -------- -------- -------- --------" \
  " -------- -------- -------- --------"

#define P1_LOG                                                                 \
  char strLog[P1_LOG_LEN] = {0};                                               \
  snprintf(strLog, P1_LOG_LEN, "Cam::%d R%d S%d E%d D%d O%d #%d", getOpenId(), \
           mTagReq.get(), mTagSet.get(), mTagEnq.get(), mTagDeq.get(),         \
           mTagOut.get(), mTagList.get());

/* P1_LOGD only use in P1NodeImp */
#if (IS_P1_LOGI)  // for system use LOGI
#define P1_LOGI(lv, fmt, arg...)                               \
  do {                                                         \
    if (mLogLevelI >= lv) {                                    \
      P1_LOG;                                                  \
      CAM_LOGI("[%s] [%s] " fmt, __FUNCTION__, strLog, ##arg); \
    }                                                          \
  } while (0)
#else
#define P1_LOGI(lv, fmt, arg...)
#endif

/* P1_LOGD only use in P1NodeImp */
#if (IS_P1_LOGD)  // for system use LOGD
#define P1_LOGD(lv, fmt, arg...)                               \
  do {                                                         \
    if (mLogLevel >= lv) {                                     \
      P1_LOG;                                                  \
      CAM_LOGD("[%s] [%s] " fmt, __FUNCTION__, strLog, ##arg); \
    }                                                          \
  } while (0)
#else
#define P1_LOGD(lv, fmt, arg...)
#endif

//
#define P1_SYS_LV_OFF (0)
#define P1_SYS_LV_BASIC (1)
#define P1_SYS_LV_CRITICAL (2)
#define P1_SYS_LV_DEFAULT (3)
#define P1_SYS_LV_VERBOSE (4)
#define P1_ATRACE_ENABLED() (0)

#define P1_CAM_TRACE_NAME_LENGTH 128
#define P1_CAM_TRACE_FMT_BEGIN(fmt, arg...)                \
  do {                                                     \
    if (CC_UNLIKELY(P1_ATRACE_ENABLED())) {                \
      char buf[P1_CAM_TRACE_NAME_LENGTH] = {0};            \
      snprintf(buf, P1_CAM_TRACE_NAME_LENGTH, fmt, ##arg); \
      CAM_TRACE_BEGIN(buf);                                \
    }                                                      \
  } while (0)

#define P1_CAM_TRACE_BEGIN(str)             \
  do {                                      \
    if (CC_UNLIKELY(P1_ATRACE_ENABLED())) { \
      CAM_TRACE_BEGIN(str);                 \
    }                                       \
  } while (0)

#define P1_CAM_TRACE_END()                  \
  do {                                      \
    if (CC_UNLIKELY(P1_ATRACE_ENABLED())) { \
      CAM_TRACE_END();                      \
    }                                       \
  } while (0)

#define P1_TRACE_S_BEGIN(lv, str) \
  do {                            \
    if (mSysLevel >= lv) {        \
      P1_CAM_TRACE_BEGIN(str);    \
    }                             \
  } while (0)

#define P1_TRACE_F_BEGIN(lv, fmt, arg...) \
  do {                                    \
    if (mSysLevel >= lv) {                \
      P1_CAM_TRACE_FMT_BEGIN(fmt, ##arg); \
    }                                     \
  } while (0)

#define P1_TRACE_C_END(lv) \
  do {                     \
    if (mSysLevel >= lv) { \
      P1_CAM_TRACE_END();  \
    }                      \
  } while (0)

class P1AutoTrace {
 public:
  inline P1AutoTrace(MINT32 sysLv, MINT32 tagLv, const char* name)
      : mExe((sysLv >= tagLv) && (P1_ATRACE_ENABLED())) {
    if (CC_UNLIKELY(mExe)) {
      CAM_TRACE_BEGIN(name);
    }
  }

  inline ~P1AutoTrace() {
    if (CC_UNLIKELY(mExe)) {
      CAM_TRACE_END();
    }
  }

 private:
  MBOOL mExe;
};

#define P1_TRACE_AUTO(lv, name) P1AutoTrace _auto_trace(mSysLevel, lv, name)

#define P1_TRACE_FUNC(lv) P1_TRACE_AUTO(lv, __FUNCTION__)

#define P1_TRACE_INT(lv, name, value)                              \
  do {                                                             \
    if (CC_UNLIKELY((mSysLevel >= lv) && (P1_ATRACE_ENABLED()))) { \
      CAM_TRACE_INT(name, value);                                  \
    }                                                              \
  } while (0)

//

#define P1THREAD_POLICY (SCHED_OTHER)
#define P1THREAD_PRIORITY \
  (NICE_CAMERA_PIPELINE_P1NODE)  // (ANDROID_PRIORITY_FOREGROUND-2)

#define P1SOFIDX_INIT_VAL (0)
#define P1SOFIDX_LAST_VAL (0xFF)
#define P1SOFIDX_NULL_VAL (0xFFFFFFFF)

#define P1_QUE_ID_NULL ((MINT32)(0))
#define P1_QUE_ID_FIRST ((MINT32)(1))
#define P1_MAGIC_NUM_INVALID ((MINT32)(-1))
#define P1_MAGIC_NUM_NULL ((MINT32)(0))
#define P1_MAGIC_NUM_FIRST ((MINT32)(1))
#define P1_FRM_NUM_NULL ((MINT32)(-1))
#define P1_REQ_NUM_NULL ((MINT32)(-1))

#define P1GET_FRM_NUM(frame) \
  ((frame == NULL) ? P1_FRM_NUM_NULL : frame->getFrameNo())
#define P1GET_REQ_NUM(frame) \
  ((frame == NULL) ? P1_REQ_NUM_NULL : frame->getRequestNo())

#if 0  // simple
#define P1NUM_ACT_STR "[%d](%d)(%d:%d)@(%d)"
#define P1INFO_ACT_STR \
  P1NUM_ACT_STR        \
  "[T:%d O:x%X R:x%X C:%d E:%d F:x%X]"
#else
#define P1NUM_ACT_STR "[Num Q:%d M:%d F:%d R:%d @%d]"
#define P1INFO_ACT_STR \
  P1NUM_ACT_STR        \
  "[Type:%d Out:x%X Rec:x%X Raw:%d Cap:%d Exe:%d Flush:x%X]"
#endif
#define P1NUM_ACT_VAR(act) \
  (act).queId, (act).magicNum, (act).frmNum, (act).reqNum, (act).sofIdx
#define P1INFO_ACT_VAR(act)                                         \
  P1NUM_ACT_VAR(act), (act).reqType, (act).reqOutSet, (act).expRec, \
      (act).fullRawType, (act).capType, (act).exeState, (act).flushSet

// #define P1INFO_ACT(LOG_LEVEL, act) MY_LOGD##LOG_LEVEL(\
//    P1INFO_ACT_STR, P1INFO_ACT_VAR(act));

#define P1INFO_STREAM_STR "StreamID(%#" PRIx64 ")[%s_%d] "

#define P1INFO_STREAM_VAR(T)                                          \
  (((mpP1NodeImp == nullptr) ||                                       \
    ((stream##T) >= (sizeof(mpP1NodeImp->mvStream##T) /               \
                     sizeof(mpP1NodeImp->mvStream##T[0]))) ||         \
    (mpP1NodeImp->mvStream##T[stream##T] == nullptr))                 \
       ? ((StreamId_T)(-1))                                           \
       : (mpP1NodeImp->mvStream##T[stream##T]->getStreamId())),       \
      (((mpP1NodeImp == nullptr) ||                                   \
        ((stream##T) >= (sizeof(mpP1NodeImp->maStream##T##Name) /     \
                         sizeof(mpP1NodeImp->maStream##T##Name[0])))) \
           ? ("UNKNOWN")                                              \
           : (mpP1NodeImp->maStream##T##Name[stream##T])),            \
      stream##T

#define P1INFO_STREAM_IMG_STR \
  P1INFO_STREAM_STR           \
  "[ImgBuf:%p-H:%p SB:%p L:%d T:%d]"
#define P1INFO_STREAM_IMG_VAR(act)                                      \
  P1INFO_STREAM_VAR(Img), (act).streamBufImg[streamImg].spImgBuf.get(), \
      (((act).streamBufImg[streamImg].spImgBuf != nullptr)              \
           ? ((act)                                                     \
                  .streamBufImg[streamImg]                              \
                  .spImgBuf->getImageBufferHeap()                       \
                  .get())                                               \
           : (nullptr)),                                                \
      (act).streamBufImg[streamImg].spStreamBuf.get(),                  \
      (act).streamBufImg[streamImg].eLockState,                         \
      (act).streamBufImg[streamImg].eSrcType

#define P1_CHECK_STREAM_SET(TYPE, stream)                               \
  if (CC_UNLIKELY(stream < (STREAM_##TYPE)STREAM_ITEM_START ||          \
                  stream >= STREAM_##TYPE##_NUM)) {                     \
    MY_LOGE("stream index invalid %d/%d", stream, STREAM_##TYPE##_NUM); \
    return INVALID_OPERATION;                                           \
  }

#define P1_CHECK_CFG_STREAM(TYPE, act, stream)                             \
  if (CC_UNLIKELY((act).mpP1NodeImp == nullptr ||                          \
                  (act).mpP1NodeImp->mvStream##TYPE[stream] == nullptr)) { \
    MY_LOGW("StreamId is NULL %d@%d", stream, (act).magicNum);             \
    return BAD_VALUE;                                                      \
  }

#define P1_CHECK_MAP_STREAM(TYPE, act, stream)                       \
  if (CC_UNLIKELY((act).appFrame == nullptr)) {                      \
    MY_LOGW("pipeline frame is NULL %d@%d", stream, (act).magicNum); \
    return INVALID_OPERATION;                                        \
  }                                                                  \
  P1_CHECK_CFG_STREAM(TYPE, (act), stream);                          \
  if (CC_UNLIKELY(!(act).streamBuf##TYPE[stream].bExist)) {          \
    MY_LOGD("stream is not exist %d@%d", stream, (act).magicNum);    \
    return OK;                                                       \
  }
//
#define P1_PORT_TO_STR(PortID)                                   \
  ((PortID.index == PORT_RRZO.index)                             \
       ? "RRZ"                                                   \
       : ((PortID.index == PORT_IMGO.index)                      \
              ? "IMG"                                            \
              : ((PortID.index == PORT_LCSO.index)               \
                     ? "LCS"                                     \
                     : ((PortID.index == PORT_RSSO.index)        \
                            ? "RSS"                              \
                            : ((PortID.index == PORT_EISO.index) \
                                   ? "EIS"                       \
                                   : "UNKNOWN")))))
//
#define P1_RECT_STR "(%d,%d_%dx%d) "
#define P1_RECT_VAR(rect) rect.p.x, rect.p.y, rect.s.w, rect.s.h
#define P1_SIZE_STR "(%dx%d) "
#define P1_SIZE_VAR(size) size.w, size.h
#define P1_STREAM_NAME_LEN (16)

#define EN_BURST_MODE (mBurstNum > 1)
#define EN_START_CAP (mEnableCaptureFlow)
#define EN_INIT_REQ_CFG \
  (mInitReqSet > 0)  // Enable InitReq Flow as configure (it might not run since
                     // mInitReqOff)
#define EN_INIT_REQ_RUN \
  (EN_INIT_REQ_CFG && (!mInitReqOff))  // InitReq Flow Enabled and Running
#define EN_REPROCESSING                             \
  (mvStreamImg[STREAM_IMG_IN_OPAQUE] != nullptr) || \
      (mvStreamImg[STREAM_IMG_IN_YUV] != nullptr)

#define IS_BURST_ON (EN_BURST_MODE)
#define IS_BURST_OFF (!EN_BURST_MODE)

#define P1NODE_DEF_SHUTTER_DELAY (2)
#define P1NODE_DEF_PROCESS_DEPTH (3)
#define P1NODE_DEF_QUEUE_DEPTH (8)
#define P1NODE_IMG_BUF_PLANE_CNT_MAX (3)
#define P1NODE_FRAME_NOTE_SLOT_SIZE_DEF (16)
#define P1NODE_FRAME_NOTE_NUM_UNKNOWN ((MINT32)(-1))
#define P1NODE_START_READY_WAIT_CNT_MAX \
  (100)  // * P1NODE_START_READY_WAIT_INV_NS
#define P1NODE_START_READY_WAIT_INV_NS (10000000LL)  // 10ms
#define P1NODE_TRANSFER_JOB_WAIT_CNT_MAX (100)  // * P1NODE_COMMON_WAIT_INV_NS
#define P1NODE_TRANSFER_JOB_WAIT_INV_NS (10000000LL)  // 10ms
#define P1NODE_COMMON_WAIT_CNT_MAX (100)  // * P1NODE_COMMON_WAIT_INV_NS
#define P1NODE_COMMON_MAGICNUM_MASK \
  (0x40000000)  // * P1NODE_COMMON_MAGICNUM_MASK
#define P1NODE_COMMON_WAIT_INV_NS (100000000LL)     // 100ms
#define P1NODE_EVT_DRAIN_WAIT_INV_NS (500000000LL)  // 500ms
#define P1_PERIODIC_INSPECT_INV_NS (3000000000LL)   // 3000ms // 3sec
#define P1_COMMON_CHECK_INV_NS (1000000000LL)       // 1000ms // 1sec
#define P1_QUE_TIMEOUT_CHECK_NS (1000000000LL)      // 1000ms // 1sec
#define P1_DELIVERY_CHECK_INV_NS (2000000000LL)     // 2000ms // 2sec
#define P1_START_CHECK_INV_NS (3000000000LL)        // 3000ms // 3sec
#define P1_CAPTURE_CHECK_INV_NS (4000000000LL)      // 4000ms // 4sec
#define P1_GENERAL_OP_TIMEOUT_US (100000LL)         // 100ms
#define P1_GENERAL_WAIT_OVERTIME_US (500000LL)      // 500ms
#define P1_GENERAL_STUCK_JUDGE_US (800000LL)        // 800ms
#define P1_GENERAL_API_CHECK_US (1000000LL)         // 1000ms // 1sec

#define P1_NOTE_SLEEP(str, ms)                      \
  {                                                 \
    MY_LOGW("[%s] NOTE_SLEEP(%d ms) +++", str, ms); \
    usleep(ms * 1000);                              \
    MY_LOGW("[%s] NOTE_SLEEP(%d ms) ---", str, ms); \
  };

#define ONE_MS_TO_NS (1000000LL)
#define ONE_US_TO_NS (1000LL)
#define ONE_S_TO_US (1000000LL)

/******************************************************************************
 *
 ******************************************************************************/
#define NEED_LOCK(need, mutex) \
  if (need) {                  \
    mutex.lock();              \
  }
#define NEED_UNLOCK(need, mutex) \
  if (need) {                    \
    mutex.unlock();              \
  }

#define P1_FILL_BYTE (0xFF)  // byte

#define CHECK_LAST_FRAME_SKIPPED(LAST_SOF_IDX, THIS_SOF_IDX) \
  ((LAST_SOF_IDX == P1SOFIDX_NULL_VAL)                       \
       ? (true)                                              \
       : ((LAST_SOF_IDX == P1SOFIDX_LAST_VAL)                \
              ? ((THIS_SOF_IDX != 0) ? (true) : (false))     \
              : ((THIS_SOF_IDX != (LAST_SOF_IDX + 1)) ? (true) : (false))));

#define RESIZE_RATIO_MAX_10X (4)
#define RESIZE_RATIO_MAX_100X (25)
#define P1_EISO_MIN_HEIGHT (160)
#define P1_RSSO_MIN_HEIGHT (22)
#define P1_RRZO_MIN_HEIGHT (2)

#define P1_STUFF_BUF_HEIGHT(rrzo, config)                                    \
  ((rrzo)                                                                    \
       ? (MAX(((IS_PORT(CONFIG_PORT_EISO, config)) ? (P1_EISO_MIN_HEIGHT)    \
                                                   : (P1_RRZO_MIN_HEIGHT)),  \
              ((IS_PORT(CONFIG_PORT_RSSO, config)) ? (P1_RSSO_MIN_HEIGHT)    \
                                                   : (P1_RRZO_MIN_HEIGHT)))) \
       : (1))
//
#define P1_IMGO_DEF_FMT (eImgFmt_BAYER10)
#define P1_PRESET_KEY_NULL (0)
#define P1NODE_METADATA_INVALID_VALUE (-1)

#define P1_STRIDE(planes, n) \
  ((planes.size() > n) ? (planes[n].rowStrideInBytes) : (0))

#define IS_RAW_FMT_PACK_FULL(fmt)                   \
  ((((EImageFormat)fmt == eImgFmt_BAYER14_UNPAK) || \
    ((EImageFormat)fmt == eImgFmt_BAYER12_UNPAK) || \
    ((EImageFormat)fmt == eImgFmt_BAYER10_UNPAK) || \
    ((EImageFormat)fmt == eImgFmt_BAYER8_UNPAK))    \
       ? (MFALSE)                                   \
       : (MTRUE))

#define P1NODE_RES_CON_ACQUIRE(Ctrl, Client, Got)                       \
  {                                                                     \
    P1NODE_RES_CON_RETURN(Ctrl, Client);                                \
    if (Got == MFALSE) {                                                \
      Client = Ctrl->requestClient();                                   \
      if (Client != IResourceConcurrency::CLIENT_HANDLER_NULL) {        \
        MY_LOGI("[ResCon][%p-%d] resource acquiring", Ctrl.get(),       \
                (MUINT32)Client);                                       \
        CAM_TRACE_FMT_BEGIN("P1:Res-Acquire[%p-%d]", Ctrl.get(),        \
                            (MUINT32)Client);                           \
        MERROR res = Ctrl->acquireResource(Client);                     \
        CAM_TRACE_FMT_END();                                            \
        if (res == NO_ERROR) {                                          \
          MY_LOGI("[ResCon][%p-%d] resource acquired (%d)", Ctrl.get(), \
                  (MUINT32)Client, res);                                \
          Got = MTRUE;                                                  \
        } else {                                                        \
          MY_LOGI("[ResCon][%p-%d] cannot acquire (%d)", Ctrl.get(),    \
                  (MUINT32)Client, res);                                \
          Got = MFALSE;                                                 \
          P1NODE_RES_CON_RETURN(Ctrl, Client);                          \
        }                                                               \
      } else {                                                          \
        MY_LOGI("[ResCon][%p-%d] cannot request", Ctrl.get(),           \
                (MUINT32)Client);                                       \
        Got = MFALSE;                                                   \
      }                                                                 \
    }                                                                   \
  }

#define P1NODE_RES_CON_RELEASE(Ctrl, Client, Got)                       \
  {                                                                     \
    if (Got == MTRUE) {                                                 \
      if (Client != IResourceConcurrency::CLIENT_HANDLER_NULL) {        \
        MY_LOGI("[ResCon][%p-%d] resource releasing", Ctrl.get(),       \
                (MUINT32)Client);                                       \
        CAM_TRACE_FMT_BEGIN("P1:Res-Release[%p-%d]", Ctrl.get(),        \
                            (MUINT32)Client);                           \
        MERROR res = Ctrl->releaseResource(Client);                     \
        CAM_TRACE_FMT_END();                                            \
        if (res == NO_ERROR) {                                          \
          MY_LOGI("[ResCon][%p-%d] resource released (%d)", Ctrl.get(), \
                  (MUINT32)Client, res);                                \
        } else {                                                        \
          MY_LOGI("[ResCon][%p-%d] cannot release (%d)", Ctrl.get(),    \
                  (MUINT32)Client, res);                                \
        }                                                               \
      }                                                                 \
      Got = MFALSE;                                                     \
    }                                                                   \
    P1NODE_RES_CON_RETURN(Ctrl, Client);                                \
  }

#define P1NODE_RES_CON_RETURN(Ctrl, Client)                         \
  {                                                                 \
    if (Client != IResourceConcurrency::CLIENT_HANDLER_NULL) {      \
      MERROR res = Ctrl->returnClient(Client);                      \
      if (res == NO_ERROR) {                                        \
        MY_LOGI("[ResCon][%p-%d] client returned (%d)", Ctrl.get(), \
                (MUINT32)Client, res);                              \
      } else {                                                      \
        MY_LOGI("[ResCon][%p-%d] cannot return (%d)", Ctrl.get(),   \
                (MUINT32)Client, res);                              \
      }                                                             \
      Client = IResourceConcurrency::CLIENT_HANDLER_NULL;           \
    }                                                               \
  }

#define P1_LOG_META(act, meta, info)                              \
  if (mMetaLogOp > 0) {                                           \
    std::string str("[P1Meta]");                                  \
    str += base::StringPrintf("[%s]", info);                      \
    str += base::StringPrintf("[Cam::%d]", getOpenId());          \
    str += base::StringPrintf(P1NUM_ACT_STR, P1NUM_ACT_VAR(act)); \
    logMeta(mMetaLogOp, meta, str.c_str(), mMetaLogTag);          \
  }
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/
typedef NS3Av3::IHal3A IHal3A_T;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/

enum START_STATE {
  START_STATE_NULL = 0,
  START_STATE_DRV_START,
  START_STATE_CAP_MANUAL_ENQ,
  START_STATE_LMV_SENSOR_EN,
  START_STATE_READY
};

enum EXE_STATE {
  EXE_STATE_NULL = 0,
  EXE_STATE_REQUESTED,
  EXE_STATE_PROCESSING,
  EXE_STATE_DONE
};
//
#if 1  // SUPPORT_UNI_SWITCH
enum UNI_SWITCH_STATE {
  UNI_SWITCH_STATE_NONE = 0,  // no UNI switch-out request
  UNI_SWITCH_STATE_REQ,  // received the switch-out request, need to switch-out
  UNI_SWITCH_STATE_ACT_ACCEPT,  // UNI is held and it will switch-out
  UNI_SWITCH_STATE_ACT_IGNORE,  // UNI is not held, ignore this switch-out
  UNI_SWITCH_STATE_ACT_REJECT   // UNI is switching and reject this switch-out
};
#endif
//
#if 1  // SUPPORT_TG_SWITCH
enum TG_SWITCH_STATE {
  TG_SWITCH_STATE_NONE = 0,     // no TG switch request
  TG_SWITCH_STATE_REQ,          // received the TG switch request
  TG_SWITCH_STATE_DONE_ACCEPT,  // TG switch command done and it accept
  TG_SWITCH_STATE_DONE_IGNORE,  // TG switch command done and it ignore
  TG_SWITCH_STATE_DONE_REJECT   // TG switch command done and it reject
};
#endif
//
#if 1  // SUPPORT_QUALITY_SWITCH
#define QUALITY_SWITCH_STATE_REQ_NON 0x80
#define QUALITY_SWITCH_STATE_REQ_H_A 0x40
#define QUALITY_SWITCH_STATE_REQ_H_B 0x20
enum QUALITY_SWITCH_STATE {
  QUALITY_SWITCH_STATE_NONE = 0,      // no QUALITY switch request
  QUALITY_SWITCH_STATE_DONE_ACCEPT,   // QUALITY switch command done and it
                                      // accept
  QUALITY_SWITCH_STATE_DONE_IGNORE,   // QUALITY switch command done and it
                                      // ignore
  QUALITY_SWITCH_STATE_DONE_REJECT,   // QUALITY switch command done and it
                                      // reject
  QUALITY_SWITCH_STATE_DONE_ILLEGAL,  // QUALITY switch command not accepted
                                      // since switching

  // received the QUALITY switch request
  QUALITY_SWITCH_STATE_REQ_L_L  // 0x80
  = (QUALITY_SWITCH_STATE_REQ_NON),
  QUALITY_SWITCH_STATE_REQ_L_H  // 0xA0 = 0x80 | 0x20
  = (QUALITY_SWITCH_STATE_REQ_NON | QUALITY_SWITCH_STATE_REQ_H_B),
  QUALITY_SWITCH_STATE_REQ_H_L  // 0xC0 = 0x80 | 0x40
  = (QUALITY_SWITCH_STATE_REQ_NON | QUALITY_SWITCH_STATE_REQ_H_A),
  QUALITY_SWITCH_STATE_REQ_H_H  // 0xE0 = 0x80 | 0x40 | 0x20
  = (QUALITY_SWITCH_STATE_REQ_NON | QUALITY_SWITCH_STATE_REQ_H_A |
     QUALITY_SWITCH_STATE_REQ_H_B)
};
#endif
//
#if 1  // SUPPORT_SENSOR_STATUS_CONTROL // for standby mode
enum SENSOR_STATUS_CTRL {
  SENSOR_STATUS_CTRL_NONE = 0,  // no sensor status control request
  SENSOR_STATUS_CTRL_STANDBY,   // received the STANDBY request
  SENSOR_STATUS_CTRL_STREAMING  // received the STREAMING request
};
#endif
//
enum REQ_REV_RES {  // Request Receive Result
  REQ_REV_RES_UNKNOWN = 0,
  REQ_REV_RES_ACCEPT_AVAILABLE,
  REQ_REV_RES_ACCEPT_BYPASS,
  REQ_REV_RES_REJECT_NOT_AVAILABLE,
  REQ_REV_RES_REJECT_NO_IO_MAP_SET,
  REQ_REV_RES_REJECT_IO_PIPE_EVT,
  REQ_REV_RES_MAX
};
//
enum ACT_TYPE {
  ACT_TYPE_NULL = 0,
  ACT_TYPE_NORMAL,
  ACT_TYPE_INTERNAL,
  ACT_TYPE_BYPASS
};
//
enum REQ_TYPE {
  REQ_TYPE_UNKNOWN = 0,
  REQ_TYPE_NORMAL,
  REQ_TYPE_INITIAL,
  REQ_TYPE_DUMMY,
  REQ_TYPE_PADDING,
  REQ_TYPE_REDO,
  REQ_TYPE_YUV,
  REQ_TYPE_ZSL
};
//
#define REQ_SET(bit) ((MUINT32)(0x1 << bit))
#define REQ_SET_NONE (0x0)
#define IS_OUT(out, set) ((set & REQ_SET(out)) == REQ_SET(out))
enum REQ_OUT {
  // @ bit
  REQ_OUT_RESIZER = 0,    // 0x 01
  REQ_OUT_RESIZER_STUFF,  // 1     // 0x 02
  REQ_OUT_LCSO,           // 2     // 0x 04
  REQ_OUT_LCSO_STUFF,     // 3     // 0x 08
  REQ_OUT_FULL_PURE,      // 4     // 0x 10
  REQ_OUT_FULL_PROC,      // 5     // 0x 20
  REQ_OUT_FULL_OPAQUE,    // 6     // 0x 40
  REQ_OUT_FULL_STUFF,     // 7     // 0x 80
  REQ_OUT_RSSO,           // 8     // 0x 0100
  REQ_OUT_RSSO_STUFF,     // 9     // 0x 0200
  REQ_OUT_MAX
};
//
#define EXP_REC(bit) ((MUINT32)(0x1 << bit))
#define EXP_REC_NONE (0x0)
#define IS_EXP(exp, rec) ((rec & EXP_REC(exp)) == EXP_REC(exp))
enum EXP_EVT {
  EXP_EVT_UNKNOWN = 0,
  EXP_EVT_NOBUF_RRZO,
  EXP_EVT_NOBUF_IMGO,
  EXP_EVT_NOBUF_EISO,
  EXP_EVT_NOBUF_LCSO,
  EXP_EVT_NOBUF_RSSO,
  EXP_EVT_MAX
};
//
#define P1_PORT_BUF_IDX_NONE \
  (0xFFFFFFFF)  // MUINT32 (4 bytes with P1_FILL_BYTE)
#define P1_META_GENERAL_EMPTY_INT ((MINT32)(-1))
//
enum P1_OUTPUT_PORT {
  P1_OUTPUT_PORT_RRZO = 0,
  P1_OUTPUT_PORT_IMGO,  // 1
  P1_OUTPUT_PORT_EISO,  // 2
  P1_OUTPUT_PORT_LCSO,  // 3
  P1_OUTPUT_PORT_RSSO,  // 4
  P1_OUTPUT_PORT_TOTAL  // (max:32) for CONFIG_PORT (MUINT32)
};
//
#define IS_PORT(port, set) ((set & port) == port)
enum CONFIG_PORT {
  CONFIG_PORT_NONE = (0x0),
  CONFIG_PORT_RRZO = (0x1 << P1_OUTPUT_PORT_RRZO),  // 0x01
  CONFIG_PORT_IMGO = (0x1 << P1_OUTPUT_PORT_IMGO),  // 0x02
  CONFIG_PORT_EISO = (0x1 << P1_OUTPUT_PORT_EISO),  // 0x04
  CONFIG_PORT_LCSO = (0x1 << P1_OUTPUT_PORT_LCSO),  // 0x08
  CONFIG_PORT_RSSO = (0x1 << P1_OUTPUT_PORT_RSSO),  // 0x10
  CONFIG_PORT_ALL = (0xFFFFFFFF)                    // MUINT32
};
//
enum ENQ_TYPE { ENQ_TYPE_NORMAL = 0, ENQ_TYPE_INITIAL, ENQ_TYPE_DIRECTLY };
//
enum P1_FLUSH_REASON {
  P1_FLUSH_REASON_GENERAL = 0,
  P1_FLUSH_REASON_PROCEDURE_FAIL,      // 1
  P1_FLUSH_REASON_OPERATION_INACTIVE,  // 2
  P1_FLUSH_REASON_NOTIFICATION_DROP,   // 3
  P1_FLUSH_REASON_INTERNAL_INITIAL,    // 4
  P1_FLUSH_REASON_INTERNAL_PADDING,    // 5
  P1_FLUSH_REASON_INTERNAL_DUMMY,      // 6
  P1_FLUSH_REASON_BYPASS_ABANDON,      // 7
  P1_FLUSH_REASON_TERMINAL_COLLECTOR,  // 8
  P1_FLUSH_REASON_TERMINAL_REQUESTQ,   // 9
  P1_FLUSH_REASON_TERMINAL_PROCESSQ,   // 10
  P1_FLUSH_REASON_REQUEST_KICK,        // 11
  P1_FLUSH_REASON_MISMATCH_EXP,        // 12
  P1_FLUSH_REASON_MISMATCH_UNCERTAIN,  // 13
  P1_FLUSH_REASON_MISMATCH_BUFFER,     // 14
  P1_FLUSH_REASON_MISMATCH_RAW,        // 15
  P1_FLUSH_REASON_MISMATCH_RESULT,     // 16
  P1_FLUSH_REASON_MISMATCH_RESIZE,     // 17
  P1_FLUSH_REASON_MISMATCH_READOUT,    // 18
  P1_FLUSH_REASON_MISMATCH_SYNC,       // 18
  P1_FLUSH_REASON_TOTAL                // (max:32) for FLUSH_TYPE (MUINT32)
};
#define IS_FLUSH(type, set) ((set & type) > 0)  // return true as one type match
enum FLUSH_TYPE {
  FLUSH_NONEED = (0x0),
  FLUSH_GENERAL = (0x1 << P1_FLUSH_REASON_GENERAL),              // 0x1
  FLUSH_FAIL = (0x1 << P1_FLUSH_REASON_PROCEDURE_FAIL),          // 0x2
  FLUSH_INACTIVE = (0x1 << P1_FLUSH_REASON_OPERATION_INACTIVE),  // 0x4
  FLUSH_DROP = (0x1 << P1_FLUSH_REASON_NOTIFICATION_DROP),       // 0x8
  FLUSH_INITIAL = (0x1 << P1_FLUSH_REASON_INTERNAL_INITIAL),     // 0x10
  FLUSH_PADDING = (0x1 << P1_FLUSH_REASON_INTERNAL_PADDING),     // 0x20
  FLUSH_DUMMY = (0x1 << P1_FLUSH_REASON_INTERNAL_DUMMY),         // 0x40
  FLUSH_INTERNAL =
      (FLUSH_INITIAL | FLUSH_PADDING | FLUSH_DUMMY),  // 0x10|0x20|0x40 = 0x70
  FLUSH_ABANDON = (0x1 << P1_FLUSH_REASON_BYPASS_ABANDON),        // 0x80
  FLUSH_COLLECTOR = (0x1 << P1_FLUSH_REASON_TERMINAL_COLLECTOR),  // 0x100
  FLUSH_REQUESTQ = (0x1 << P1_FLUSH_REASON_TERMINAL_REQUESTQ),    // 0x200
  FLUSH_PROCESSQ = (0x1 << P1_FLUSH_REASON_TERMINAL_PROCESSQ),    // 0x400
  FLUSH_TERMINAL = (FLUSH_COLLECTOR | FLUSH_REQUESTQ |
                    FLUSH_PROCESSQ),  // 0x100|0x200|0x400 = 0x700
  FLUSH_KICK = (0x1 << P1_FLUSH_REASON_REQUEST_KICK),                 // 0x800
  FLUSH_MIS_EXP = (0x1 << P1_FLUSH_REASON_MISMATCH_EXP),              // 0x1000
  FLUSH_MIS_UNCERTAIN = (0x1 << P1_FLUSH_REASON_MISMATCH_UNCERTAIN),  // 0x2000
  FLUSH_MIS_BUFFER = (0x1 << P1_FLUSH_REASON_MISMATCH_BUFFER),        // 0x4000
  FLUSH_MIS_RAW = (0x1 << P1_FLUSH_REASON_MISMATCH_RAW),              // 0x8000
  FLUSH_MIS_RESULT = (0x1 << P1_FLUSH_REASON_MISMATCH_RESULT),        // 0x10000
  FLUSH_MIS_RESIZE = (0x1 << P1_FLUSH_REASON_MISMATCH_RESIZE),        // 0x20000
  FLUSH_MIS_READOUT = (0x1 << P1_FLUSH_REASON_MISMATCH_READOUT),      // 0x40000
  FLUSH_MIS_SYNC = (0x1 << P1_FLUSH_REASON_MISMATCH_SYNC),            // 0x80000
  FLUSH_ALL = (0xFFFFFFFF)                                            // MUINT32
};
//
enum PREPARE_CROP_PHASE {
  PREPARE_CROP_PHASE_RECEIVE_CREATE,
  PREPARE_CROP_PHASE_CONTROL_RESIZE,
};
//
enum INFLIGHT_MONITORING_TIMING {
  IMT_COMMON = 0,
  IMT_REQ,
  IMT_ENQ,
  IMT_DEQ,
  IMT_MAX
};
//
#define BIN_RESIZE(x) (x = (x >> 1))
#define BIN_REVERT(x) (x = (x << 1))

//
#define STREAM_ITEM_START (0)
//
enum STREAM_IMG {
  STREAM_IMG_IN_YUV = STREAM_ITEM_START,
  STREAM_IMG_IN_OPAQUE,
  STREAM_IMG_OUT_OPAQUE,
  STREAM_IMG_OUT_FULL,
  STREAM_IMG_OUT_RESIZE,
  STREAM_IMG_OUT_LCS,
  STREAM_IMG_OUT_RSS,
  STREAM_IMG_NUM
};
#define STREAM_IMG_IN_BGN STREAM_IMG_IN_YUV
#define STREAM_IMG_IN_END STREAM_IMG_IN_OPAQUE
#define IS_IN_STREAM_IMG(img) \
  ((img == STREAM_IMG_IN_YUV) || (img == STREAM_IMG_IN_OPAQUE))
//
enum STREAM_META {
  STREAM_META_IN_APP = STREAM_ITEM_START,
  STREAM_META_IN_HAL,
  STREAM_META_OUT_APP,
  STREAM_META_OUT_HAL,
  STREAM_META_NUM
};
#define IS_IN_STREAM_META(meta) \
  ((meta == STREAM_META_IN_APP) || (meta == STREAM_META_IN_HAL))

//
enum IMG_BUF_SRC {
  IMG_BUF_SRC_NULL = 0,
  IMG_BUF_SRC_POOL,
  IMG_BUF_SRC_STUFF,
  IMG_BUF_SRC_FRAME
};

enum SLG {                     // Sys-Level-Group
  SLG_OFF = P1_SYS_LV_OFF,     // forced-off
  SLG_B = P1_SYS_LV_BASIC,     // basic basis base
  SLG_E = P1_SYS_LV_BASIC,     // event or exception
  SLG_S = P1_SYS_LV_CRITICAL,  // start/stop significance
  SLG_R = P1_SYS_LV_DEFAULT,   // start/stop reference
  SLG_I = P1_SYS_LV_DEFAULT,   // inflight information
  SLG_O = P1_SYS_LV_VERBOSE,   // others
  SLG_PFL = P1_SYS_LV_BASIC    // per-frame-log // it should not present in
                               // performance checking by log level control
};

};  // namespace NSP1Node
};  // namespace v3
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1COMMON_H_
