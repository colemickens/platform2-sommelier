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

#define LOG_TAG "mtkcam-PipelineContextBuilder"
#include <impl/PipelineContextBuilder.h>
#include <memory>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/hw/HwInfoHelper.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/pipeline/hwnode/FDNode.h>
#include <mtkcam/pipeline/hwnode/JpegNode.h>
#include <mtkcam/pipeline/hwnode/NodeId.h>
#include <mtkcam/pipeline/hwnode/P1Node.h>
#include <mtkcam/pipeline/hwnode/P2StreamingNode.h>
#include <mtkcam/pipeline/hwnode/P2CaptureNode.h>
#include <mtkcam/pipeline/hwnode/StreamId.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
#include <mtkcam/pipeline/utils/SyncHelper/ISyncHelper.h>
#include <vector>

#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/
// using namespace android;

using NSCam::v3::eSTREAMTYPE_IMAGE_OUT;
using NSCam::v3::JpegNode;
using NSCam::v3::NodeId_T;
using NSCam::v3::NodeSet;
using NSCam::v3::P1Node;
using NSCam::v3::P2CaptureNode;
using NSCam::v3::P2StreamingNode;
using NSCam::v3::NSPipelineContext::eStreamType_IMG_APP;
using NSCam::v3::NSPipelineContext::eStreamType_IMG_HAL_POOL;
using NSCam::v3::NSPipelineContext::eStreamType_IMG_HAL_PROVIDER;
using NSCam::v3::NSPipelineContext::eStreamType_IMG_HAL_RUNTIME;
using NSCam::v3::NSPipelineContext::eStreamType_META_APP;
using NSCam::v3::NSPipelineContext::eStreamType_META_HAL;
using NSCam::v3::NSPipelineContext::NodeActor;
using NSCam::v3::NSPipelineContext::NodeBuilder;
using NSCam::v3::NSPipelineContext::NodeEdgeSet;
using NSCam::v3::NSPipelineContext::PipelineBuilder;
using NSCam::v3::NSPipelineContext::PipelineContext;
using NSCam::v3::NSPipelineContext::StreamSet;
/******************************************************************************
 *
 ******************************************************************************/
#define CHECK_ERROR(_err_, _ops_)                                 \
  do {                                                            \
    int const err = (_err_);                                      \
    if (CC_UNLIKELY(err != 0)) {                                  \
      MY_LOGE("err:%d(%s) ops:%s", err, ::strerror(-err), _ops_); \
      return err;                                                 \
    }                                                             \
  } while (0)

/******************************************************************************
 *
 ******************************************************************************/
namespace {

struct InternalCommonInputParams {
  NSCam::v3::pipeline::model::PipelineStaticInfo const* pPipelineStaticInfo =
      nullptr;
  NSCam::v3::pipeline::model::PipelineUserConfiguration const*
      pPipelineUserConfiguration = nullptr;
  bool const bIsReConfigure;
};

}  // namespace

/******************************************************************************
 *
 ******************************************************************************/
static int configContextLocked_Streams(
    std::shared_ptr<NSCam::v3::NSPipelineContext::PipelineContext> pContext,
    std::vector<NSCam::v3::pipeline::model::ParsedStreamInfo_P1> const*
        pParsedStreamInfo_P1,
    std::shared_ptr<NSCam::v3::Utils::IStreamBufferProvider> pZSLProvider,
    NSCam::v3::pipeline::model::ParsedStreamInfo_NonP1 const*
        pParsedStreamInfo_NonP1,
    NSCam::v3::pipeline::model::PipelineUserConfiguration const*
        pPipelineUserConfiguration) {
  FUNC_START;
  CAM_TRACE_CALL();
#define BuildStream(_type_, _IStreamInfo_)                           \
  do {                                                               \
    if (_IStreamInfo_) {                                             \
      int err = 0;                                                   \
      if (0 != (err = NSCam::v3::NSPipelineContext::StreamBuilder(   \
                          _type_, _IStreamInfo_)                     \
                          .build(pContext))) {                       \
        MY_LOGE("StreamBuilder fail stream %#" PRIx64 " of type %d", \
                _IStreamInfo_->getStreamId(), _type_);               \
        return err;                                                  \
      }                                                              \
    }                                                                \
  } while (0)

#define BuildStreamWithProvider(_type_, _IStreamInfo_, _buf_provider_) \
  do {                                                                 \
    if (_buf_provider_) {                                              \
      if (_IStreamInfo_) {                                             \
        int err = 0;                                                   \
        if (0 != (err = NSCam::v3::NSPipelineContext::StreamBuilder(   \
                            _type_, _IStreamInfo_)                     \
                            .setProvider(_buf_provider_)               \
                            .build(pContext))) {                       \
          MY_LOGE("StreamBuilder fail stream %#" PRIx64 " of type %d", \
                  _IStreamInfo_->getStreamId(), _type_);               \
          return err;                                                  \
        }                                                              \
      }                                                                \
    } else {                                                           \
      BuildStream(_type_, _IStreamInfo_);                              \
    }                                                                  \
  } while (0)

  auto const& pParsedAppImageStreamInfo =
      pPipelineUserConfiguration->pParsedAppImageStreamInfo;
  // Non-P1
  // app meta stream
  BuildStream(eStreamType_META_APP, pParsedStreamInfo_NonP1->pAppMeta_Control);
  BuildStream(eStreamType_META_APP,
              pParsedStreamInfo_NonP1->pAppMeta_DynamicP2StreamNode);
  BuildStream(eStreamType_META_APP,
              pParsedStreamInfo_NonP1->pAppMeta_DynamicP2CaptureNode);
  BuildStream(eStreamType_META_APP,
              pParsedStreamInfo_NonP1->pAppMeta_DynamicFD);
  BuildStream(eStreamType_META_APP,
              pParsedStreamInfo_NonP1->pAppMeta_DynamicJpeg);
  // hal meta stream
  BuildStream(eStreamType_META_HAL,
              pParsedStreamInfo_NonP1->pHalMeta_DynamicP2StreamNode);
  BuildStream(eStreamType_META_HAL,
              pParsedStreamInfo_NonP1->pHalMeta_DynamicP2CaptureNode);
  BuildStream(eStreamType_META_HAL,
              pParsedStreamInfo_NonP1->pHalMeta_DynamicPDE);

  // hal image stream
  BuildStream(eStreamType_IMG_HAL_POOL,
              pParsedStreamInfo_NonP1->pHalImage_FD_YUV);

  int32_t enable = property_get_int32("vendor.jpeg.rotation.enable", 1);
  MY_LOGD("Jpeg Rotation enable: %d", enable);
  if (!(enable & 0x1)) {
    BuildStream(eStreamType_IMG_HAL_POOL,
                pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV);
  } else {
    BuildStream(eStreamType_IMG_HAL_RUNTIME,
                pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV);
  }

  BuildStream(eStreamType_IMG_HAL_RUNTIME,
              pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV);

  // P1
  for (size_t i = 0; i < (*pParsedStreamInfo_P1).size(); i++) {
    auto& info = (*pParsedStreamInfo_P1)[i];
    MY_LOGD("index : %zu", i);
    BuildStream(eStreamType_META_APP, info.pAppMeta_DynamicP1);
    BuildStream(eStreamType_META_HAL, info.pHalMeta_Control);
    BuildStream(eStreamType_META_HAL, info.pHalMeta_DynamicP1);
    MY_LOGD("Build P1 stream");
    if (pZSLProvider) {
      BuildStreamWithProvider(eStreamType_IMG_HAL_PROVIDER,
                              info.pHalImage_P1_Imgo, pZSLProvider);
      BuildStreamWithProvider(eStreamType_IMG_HAL_PROVIDER,
                              info.pHalImage_P1_Rrzo, pZSLProvider);
      BuildStreamWithProvider(eStreamType_IMG_HAL_PROVIDER,
                              info.pHalImage_P1_Lcso, pZSLProvider);
    } else {
      BuildStream(eStreamType_IMG_HAL_POOL /*eStreamType_IMG_HAL_RUNTIME*/,
                  info.pHalImage_P1_Imgo);
      BuildStream(eStreamType_IMG_HAL_POOL, info.pHalImage_P1_Rrzo);
      BuildStream(eStreamType_IMG_HAL_POOL, info.pHalImage_P1_Lcso);
    }
    BuildStream(eStreamType_IMG_HAL_POOL, info.pHalImage_P1_Rsso);
    MY_LOGD("New: p1 full raw(%p); resized raw(%p), pZSLProvider(%p)",
            info.pHalImage_P1_Imgo.get(), info.pHalImage_P1_Rrzo.get(),
            pZSLProvider.get());
  }

  //  app image stream
  for (const auto& n : pParsedAppImageStreamInfo->vAppImage_Output_Proc) {
    BuildStream(eStreamType_IMG_APP, n.second);
  }
  BuildStream(eStreamType_IMG_APP,
              pParsedAppImageStreamInfo->pAppImage_Input_Yuv);
  BuildStream(eStreamType_IMG_APP,
              pParsedAppImageStreamInfo->pAppImage_Input_Priv);
  BuildStream(eStreamType_IMG_APP,
              pParsedAppImageStreamInfo->pAppImage_Output_Priv);
  //
  BuildStream(eStreamType_IMG_APP, pParsedAppImageStreamInfo->pAppImage_Jpeg);
  //
#undef BuildStream
#undef BuildStreamWithProvider
  FUNC_END;
  //
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
#define add_stream_to_set(_set_, _IStreamInfo_) \
  do {                                          \
    if (_IStreamInfo_) {                        \
      _set_.add(_IStreamInfo_->getStreamId());  \
    }                                           \
  } while (0)
//
#define setImageUsage(_IStreamInfo_, _usg_)                             \
  do {                                                                  \
    if (_IStreamInfo_) {                                                \
      builder.setImageStreamUsage(_IStreamInfo_->getStreamId(), _usg_); \
    }                                                                   \
  } while (0)

/******************************************************************************
 *
 ******************************************************************************/
static int configContextLocked_P1Node(
    std::shared_ptr<PipelineContext> pContext,
    std::shared_ptr<PipelineContext> const& pOldPipelineContext,
    NSCam::v3::pipeline::model::StreamingFeatureSetting const*
        pStreamingFeatureSetting,
    NSCam::v3::pipeline::model::ParsedStreamInfo_P1 const* pParsedStreamInfo_P1,
    NSCam::v3::pipeline::model::ParsedStreamInfo_NonP1 const*
        pParsedStreamInfo_NonP1,
    NSCam::v3::pipeline::model::SensorSetting const* pSensorSetting,
    NSCam::v3::pipeline::policy::P1HwSetting const* pP1HwSetting,
    size_t const idx,
    uint32_t batchSize,
    bool bMultiDevice,
    InternalCommonInputParams const* pCommon) {
  typedef P1Node NodeT;
  typedef NodeActor<NodeT> NodeActorT;

  auto const& pPipelineStaticInfo = pCommon->pPipelineStaticInfo;
  auto const& pPipelineUserConfiguration = pCommon->pPipelineUserConfiguration;
  auto const& pParsedAppConfiguration =
      pPipelineUserConfiguration->pParsedAppConfiguration;
  auto const& pParsedAppImageStreamInfo =
      pPipelineUserConfiguration->pParsedAppImageStreamInfo;
  //
  auto const physicalSensorId = pPipelineStaticInfo->sensorIds[idx];
  //
  int32_t initRequest =
      property_get_int32("vendor.debug.camera.pass1initrequestnum", 0);
  //
  NodeId_T nodeId = NSCam::eNODEID_P1Node;
  if (idx == 1) {
    nodeId = NSCam::eNODEID_P1Node_main2;
  }
  //
  NodeT::InitParams initParam;
  {
    initParam.openId = physicalSensorId;
    initParam.nodeId = nodeId;
    initParam.nodeName = "P1Node";
  }

  NodeT::ConfigParams cfgParam;
  {
    NodeT::SensorParams sensorParam(
        /*mode     : */ pSensorSetting->sensorMode,
        /*size     : */ pSensorSetting->sensorSize,
        /*fps      : */ pSensorSetting->sensorFps,
        /*pixelMode: */ pP1HwSetting->pixelMode,
        /*vhdrMode : */ pStreamingFeatureSetting->vhdrMode);
    //
    cfgParam.pInAppMeta = pParsedStreamInfo_NonP1->pAppMeta_Control;
    cfgParam.pInHalMeta = pParsedStreamInfo_P1->pHalMeta_Control;
    cfgParam.pOutAppMeta = pParsedStreamInfo_P1->pAppMeta_DynamicP1;
    cfgParam.pOutHalMeta = pParsedStreamInfo_P1->pHalMeta_DynamicP1;
    cfgParam.pOutImage_resizer = pParsedStreamInfo_P1->pHalImage_P1_Rrzo;

    cfgParam.pOutImage_lcso = pParsedStreamInfo_P1->pHalImage_P1_Lcso;
    cfgParam.pOutImage_rsso = pParsedStreamInfo_P1->pHalImage_P1_Rsso;
    if (pParsedStreamInfo_P1->pHalImage_P1_Imgo) {
      cfgParam.pvOutImage_full.push_back(
          pParsedStreamInfo_P1->pHalImage_P1_Imgo);
    }
    cfgParam.enableLCS =
        pParsedStreamInfo_P1->pHalImage_P1_Lcso ? MTRUE : MFALSE;

    // for CCT dump
    {
      int debugProcRaw =
          property_get_int32("vendor.debug.camera.cfg.ProcRaw", -1);
      if (debugProcRaw > 0) {
        MY_LOGD(
            "set vendor.debug.camera.ProcRaw(%d) => 0:config pure raw  "
            "1:config processed raw",
            debugProcRaw);
        cfgParam.rawProcessed = debugProcRaw;
      }
    }
    //
    cfgParam.sensorParams = sensorParam;
    cfgParam.pStreamPool_resizer = nullptr;
    cfgParam.pStreamPool_full = nullptr;
    if (idx == 0) {
      cfgParam.pInImage_yuv = pParsedAppImageStreamInfo->pAppImage_Input_Yuv;
      cfgParam.pInImage_opaque =
          pParsedAppImageStreamInfo->pAppImage_Input_Priv;
      cfgParam.pOutImage_opaque =
          pParsedAppImageStreamInfo->pAppImage_Output_Priv;
    }
    cfgParam.sensorParams = sensorParam;
    {
      int64_t ImgoId;
      int64_t RrzoId;
      int64_t LcsoId;
      int64_t RssoId;
      switch (idx) {
        case 0:
          ImgoId = NSCam::eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00;
          RrzoId = NSCam::eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00;
          LcsoId = NSCam::eSTREAMID_IMAGE_PIPE_RAW_LCSO_00;
          RssoId = NSCam::eSTREAMID_IMAGE_PIPE_RAW_RSSO_00;
          break;
        case 1:
          ImgoId = NSCam::eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01;
          RrzoId = NSCam::eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01;
          LcsoId = NSCam::eSTREAMID_IMAGE_PIPE_RAW_LCSO_01;
          RssoId = NSCam::eSTREAMID_IMAGE_PIPE_RAW_RSSO_01;
          break;
        default:
          MY_LOGE("not support p1 node number large than 2");
          break;
      }
      cfgParam.pStreamPool_resizer =
          pParsedStreamInfo_P1->pHalImage_P1_Rrzo
              ? pContext->queryImageStreamPool(RrzoId)
              : nullptr;
      cfgParam.pStreamPool_lcso = pParsedStreamInfo_P1->pHalImage_P1_Lcso
                                      ? pContext->queryImageStreamPool(LcsoId)
                                      : nullptr;
      cfgParam.pStreamPool_rsso = pParsedStreamInfo_P1->pHalImage_P1_Rsso
                                      ? pContext->queryImageStreamPool(RssoId)
                                      : nullptr;
      cfgParam.pStreamPool_full = pParsedStreamInfo_P1->pHalImage_P1_Imgo
                                      ? pContext->queryImageStreamPool(ImgoId)
                                      : nullptr;
    }
    MBOOL needLMV = (pParsedAppImageStreamInfo->hasVideoConsumer &&
                     !pParsedAppConfiguration->operationMode) ||
                    pStreamingFeatureSetting->bNeedLMV;
    cfgParam.enableEIS = (needLMV && idx == 0) ? MTRUE : MFALSE;

    //
    if (pParsedAppImageStreamInfo->hasVideo4K) {
      cfgParam.receiveMode = NodeT::REV_MODE::REV_MODE_CONSERVATIVE;
    }
    // config initframe
    if (pCommon->bIsReConfigure) {
      MY_LOGD("Is Reconfig flow, force init request = 0");
      initRequest = 0;
    } else if (initRequest == 0) {
      if (NSCam::IMetadata::getEntry<MINT32>(
              &pParsedAppConfiguration->sessionParams,
              MTK_CONFIGURE_SETTING_INIT_REQUEST, &initRequest)) {
        MY_LOGD("APP set init frame : %d, if not be zero, force it to be 4",
                initRequest);
        if (initRequest != 0) {
          initRequest = 4;  // force 4
        }
      }
    }
    cfgParam.initRequest = initRequest;
    //
    cfgParam.burstNum = batchSize;
    //
    std::shared_ptr<NSCamHW::HwInfoHelper> infohelper =
        std::make_shared<NSCamHW::HwInfoHelper>(physicalSensorId);
    bool ret = infohelper != nullptr && infohelper->updateInfos();
    if (CC_UNLIKELY(!ret)) {
      MY_LOGE("HwInfoHelper");
    }
  }
  //
  std::shared_ptr<NodeActorT> pNode;
  {
    std::shared_ptr<NodeActorT> pOldNode;
    MBOOL bHasOldNode =
        (pOldPipelineContext &&
         NSCam::OK == pOldPipelineContext->queryNodeActor(nodeId, &pOldNode));
    if (bHasOldNode) {
      pOldNode->getNodeImpl()
          ->uninit();  // must uninit old P1 before call new P1 config
    }
    //
    pNode = std::make_shared<NodeActorT>(NodeT::createInstance());
  }
  pNode->setInitParam(initParam);
  pNode->setConfigParam(cfgParam);
  //
  StreamSet inStreamSet;
  StreamSet outStreamSet;
  //
  add_stream_to_set(inStreamSet, pParsedStreamInfo_NonP1->pAppMeta_Control);
  add_stream_to_set(inStreamSet, pParsedStreamInfo_P1->pHalMeta_Control);
  if (idx == 0) {
    add_stream_to_set(inStreamSet,
                      pParsedAppImageStreamInfo->pAppImage_Input_Yuv);
    add_stream_to_set(inStreamSet,
                      pParsedAppImageStreamInfo->pAppImage_Input_Priv);
  }
  //
  add_stream_to_set(outStreamSet, pParsedStreamInfo_P1->pHalImage_P1_Imgo);
  add_stream_to_set(outStreamSet, pParsedStreamInfo_P1->pHalImage_P1_Rrzo);
  add_stream_to_set(outStreamSet, pParsedStreamInfo_P1->pHalImage_P1_Lcso);
  add_stream_to_set(outStreamSet, pParsedStreamInfo_P1->pHalImage_P1_Rsso);
  add_stream_to_set(outStreamSet, pParsedStreamInfo_P1->pAppMeta_DynamicP1);
  add_stream_to_set(outStreamSet, pParsedStreamInfo_P1->pHalMeta_DynamicP1);
  if (idx == 0) {
    add_stream_to_set(outStreamSet,
                      pParsedAppImageStreamInfo->pAppImage_Output_Priv);
  }
  //
  NodeBuilder builder(nodeId, pNode);
  builder.addStream(NodeBuilder::eDirection_IN, inStreamSet)
      .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
  //
  if (idx == 0) {
    setImageUsage(pParsedAppImageStreamInfo->pAppImage_Input_Yuv,
                  NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage(pParsedAppImageStreamInfo->pAppImage_Input_Priv,
                  NSCam::eBUFFER_USAGE_SW_READ_OFTEN);
  }
  //
  setImageUsage(pParsedStreamInfo_P1->pHalImage_P1_Imgo,
                NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                    NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  setImageUsage(pParsedStreamInfo_P1->pHalImage_P1_Rrzo,
                NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                    NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  setImageUsage(pParsedStreamInfo_P1->pHalImage_P1_Lcso,
                NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                    NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  setImageUsage(pParsedStreamInfo_P1->pHalImage_P1_Rsso,
                NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                    NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  if (idx == 0) {
    setImageUsage(pParsedAppImageStreamInfo->pAppImage_Output_Priv,
                  NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                      NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  }
  //
  int err = builder.build(pContext);
  if (err != NSCam::OK) {
    MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
  }
  return err;
}

/******************************************************************************
 *
 ******************************************************************************/
static int configContextLocked_P2SNode(
    std::shared_ptr<PipelineContext> pContext,
    NSCam::v3::pipeline::model::StreamingFeatureSetting const*
        pStreamingFeatureSetting,
    std::vector<NSCam::v3::pipeline::model::ParsedStreamInfo_P1> const*
        pParsedStreamInfo_P1,
    NSCam::v3::pipeline::model::ParsedStreamInfo_NonP1 const*
        pParsedStreamInfo_NonP1,
    uint32_t batchSize,
    uint32_t useP1NodeCount __unused,
    bool bHasMonoSensor __unused,
    InternalCommonInputParams const* pCommon) {
  typedef P2StreamingNode NodeT;
  typedef NodeActor<NodeT> NodeActorT;
  auto const& pPipelineStaticInfo = pCommon->pPipelineStaticInfo;
  auto const& pPipelineUserConfiguration = pCommon->pPipelineUserConfiguration;
  auto const& pParsedAppConfiguration =
      pPipelineUserConfiguration->pParsedAppConfiguration;
  auto const& pParsedAppImageStreamInfo =
      pPipelineUserConfiguration->pParsedAppImageStreamInfo;
  //
  NodeId_T const nodeId = NSCam::eNODEID_P2StreamNode;
  //
  NodeT::InitParams initParam;
  {
    initParam.openId = pPipelineStaticInfo->sensorIds[0];
    initParam.nodeId = nodeId;
    initParam.nodeName = "P2StreamNode";
    for (size_t i = 1; i < useP1NodeCount; i++) {
      initParam.subOpenIdList.push_back(pPipelineStaticInfo->sensorIds[i]);
    }
  }
  NodeT::ConfigParams cfgParam;
  {
    cfgParam.pInAppMeta = pParsedStreamInfo_NonP1->pAppMeta_Control;
    cfgParam.pInAppRetMeta = (*pParsedStreamInfo_P1)[0].pAppMeta_DynamicP1;
    cfgParam.pInHalMeta = (*pParsedStreamInfo_P1)[0].pHalMeta_DynamicP1;
    cfgParam.pOutAppMeta =
        pParsedStreamInfo_NonP1->pAppMeta_DynamicP2StreamNode;
    cfgParam.pOutHalMeta =
        pParsedStreamInfo_NonP1->pHalMeta_DynamicP2StreamNode;
    //
    if ((*pParsedStreamInfo_P1)[0].pHalImage_P1_Imgo) {
      cfgParam.pvInFullRaw.push_back(
          (*pParsedStreamInfo_P1)[0].pHalImage_P1_Imgo);
    }
    //
    cfgParam.pInResizedRaw = (*pParsedStreamInfo_P1)[0].pHalImage_P1_Rrzo;
    cfgParam.vStreamConfigure.mInStreams.push_back(
        (*pParsedStreamInfo_P1)[0].pHalImage_P1_Rrzo);
    //
    cfgParam.pInLcsoRaw = (*pParsedStreamInfo_P1)[0].pHalImage_P1_Lcso;
    //
    cfgParam.pInRssoRaw = (*pParsedStreamInfo_P1)[0].pHalImage_P1_Rsso;
    //
    if (useP1NodeCount > 1) {
      cfgParam.pInAppRetMeta_Sub =
          (*pParsedStreamInfo_P1)[1].pAppMeta_DynamicP1;
      cfgParam.pInHalMeta_Sub = (*pParsedStreamInfo_P1)[1].pHalMeta_DynamicP1;
      if ((*pParsedStreamInfo_P1)[1].pHalImage_P1_Imgo) {
        cfgParam.pvInFullRaw_Sub.push_back(
            (*pParsedStreamInfo_P1)[1].pHalImage_P1_Imgo);
      }
      //
      cfgParam.pInResizedRaw_Sub = (*pParsedStreamInfo_P1)[1].pHalImage_P1_Rrzo;
      //
      cfgParam.pInLcsoRaw_Sub = (*pParsedStreamInfo_P1)[1].pHalImage_P1_Lcso;
      //
      cfgParam.pInRssoRaw_Sub = (*pParsedStreamInfo_P1)[1].pHalImage_P1_Rsso;
    }
    //
    if (pParsedAppImageStreamInfo->pAppImage_Output_Priv) {
      cfgParam.pvInOpaque.push_back(
          pParsedAppImageStreamInfo->pAppImage_Output_Priv);
    }
    //
    for (const auto& n : pParsedAppImageStreamInfo->vAppImage_Output_Proc) {
      cfgParam.vOutImage.push_back(n.second);
      // oapaue reprocessing do not add stream information to P2StreamNode
      if (!pParsedAppImageStreamInfo->pAppImage_Input_Priv &&
          !pParsedAppImageStreamInfo->pAppImage_Output_Priv) {
        cfgParam.vStreamConfigure.mOutStreams.push_back(n.second);
      }
    }
    //
    if (pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV) {
      cfgParam.vOutImage.push_back(pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV);
    }
    if (pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV) {
      cfgParam.vOutImage.push_back(
          pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV);
    }
    //
    cfgParam.pOutFDImage = pParsedStreamInfo_NonP1->pHalImage_FD_YUV;
    if (pParsedStreamInfo_NonP1->pHalImage_FD_YUV) {
      cfgParam.vStreamConfigure.mOutStreams.push_back(
          pParsedStreamInfo_NonP1->pHalImage_FD_YUV);
    }
    //
    cfgParam.burstNum = batchSize;
  }

  NSCam::v3::P2Common::UsageHint p2Usage;
  {
    p2Usage.mP2NodeType = NSCam::v3::P2Common::P2_NODE_COMMON;
    p2Usage.m3DNRMode = pStreamingFeatureSetting->nr3dMode;

    if (pParsedAppImageStreamInfo->hasVideoConsumer) {
      p2Usage.mAppMode = NSCam::v3::P2Common::APP_MODE_VIDEO;
    }
    if (pParsedAppConfiguration->operationMode ==
        1 /* CONSTRAINED_HIGH_SPEED_MODE */) {
      p2Usage.mAppMode = NSCam::v3::P2Common::APP_MODE_HIGH_SPEED_VIDEO;
    }
    if ((*pParsedStreamInfo_P1)[0].pHalImage_P1_Rrzo != nullptr) {
      p2Usage.mStreamingSize =
          (*pParsedStreamInfo_P1)[0].pHalImage_P1_Rrzo->getImgSize();
    }
    p2Usage.mOutCfg.mMaxOutNum = cfgParam.vOutImage.size();
    for (auto&& out : cfgParam.vOutImage) {
      if (out->getImgSize().w > p2Usage.mStreamingSize.w ||
          out->getImgSize().h > p2Usage.mStreamingSize.h) {
        p2Usage.mOutCfg.mHasLarge = MTRUE;
      }
    }
  }
  cfgParam.mUsageHint = p2Usage;

  //
  std::shared_ptr<NodeActorT> pNode = std::make_shared<NodeActorT>(
      NodeT::createInstance(P2StreamingNode::PASS2_STREAM, p2Usage));
  pNode->setInitParam(initParam);
  pNode->setConfigParam(cfgParam);
  //
  StreamSet inStreamSet;
  StreamSet outStreamSet;
  //
  add_stream_to_set(inStreamSet, pParsedStreamInfo_NonP1->pAppMeta_Control);
  for (size_t i = 0; i < useP1NodeCount; i++) {
    add_stream_to_set(inStreamSet,
                      (*pParsedStreamInfo_P1)[i].pAppMeta_DynamicP1);
    add_stream_to_set(inStreamSet,
                      (*pParsedStreamInfo_P1)[i].pHalMeta_DynamicP1);
    add_stream_to_set(inStreamSet,
                      (*pParsedStreamInfo_P1)[i].pHalImage_P1_Imgo);
    add_stream_to_set(inStreamSet,
                      (*pParsedStreamInfo_P1)[i].pHalImage_P1_Rrzo);
    add_stream_to_set(inStreamSet,
                      (*pParsedStreamInfo_P1)[i].pHalImage_P1_Lcso);
  }
  add_stream_to_set(inStreamSet,
                    pParsedAppImageStreamInfo->pAppImage_Output_Priv);
  //
  add_stream_to_set(outStreamSet,
                    pParsedStreamInfo_NonP1->pAppMeta_DynamicP2StreamNode);
  add_stream_to_set(outStreamSet,
                    pParsedStreamInfo_NonP1->pHalMeta_DynamicP2StreamNode);
  add_stream_to_set(outStreamSet, pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV);
  add_stream_to_set(outStreamSet,
                    pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV);
  //
  for (const auto& n : pParsedAppImageStreamInfo->vAppImage_Output_Proc) {
    add_stream_to_set(outStreamSet, n.second);
  }
  //
  add_stream_to_set(outStreamSet, pParsedStreamInfo_NonP1->pHalImage_FD_YUV);
  //
  NodeBuilder builder(nodeId, pNode);
  builder.addStream(NodeBuilder::eDirection_IN, inStreamSet)
      .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
  //
  setImageUsage(
      pParsedAppImageStreamInfo->pAppImage_Output_Priv,
      NSCam::eBUFFER_USAGE_SW_READ_OFTEN | NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
  for (size_t i = 0; i < useP1NodeCount; i++) {
    setImageUsage((*pParsedStreamInfo_P1)[i].pHalImage_P1_Imgo,
                  NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                      NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage((*pParsedStreamInfo_P1)[i].pHalImage_P1_Rrzo,
                  NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                      NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage((*pParsedStreamInfo_P1)[i].pHalImage_P1_Lcso,
                  NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                      NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
  }
  //
  for (const auto& n : pParsedAppImageStreamInfo->vAppImage_Output_Proc) {
    setImageUsage(n.second, NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                                NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  }
  //
  setImageUsage(pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV,
                NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                    NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  setImageUsage(pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV,
                NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                    NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  setImageUsage(pParsedStreamInfo_NonP1->pHalImage_FD_YUV,
                NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                    NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  //
  int err = builder.build(pContext);
  if (err != NSCam::OK) {
    MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
  }
  return err;
}

/******************************************************************************
 *
 ******************************************************************************/
static int configContextLocked_P2CNode(
    std::shared_ptr<PipelineContext> pContext,
    NSCam::v3::pipeline::model::CaptureFeatureSetting const*
        pCaptureFeatureSetting __unused,
    std::vector<NSCam::v3::pipeline::model::ParsedStreamInfo_P1> const*
        pParsedStreamInfo_P1,
    NSCam::v3::pipeline::model::ParsedStreamInfo_NonP1 const*
        pParsedStreamInfo_NonP1,
    uint32_t useP1NodeCount,
    InternalCommonInputParams const* pCommon) {
  typedef P2CaptureNode NodeT;
  typedef NodeActor<NodeT> NodeActorT;

  auto const& pPipelineStaticInfo = pCommon->pPipelineStaticInfo;
  auto const& pPipelineUserConfiguration = pCommon->pPipelineUserConfiguration;
  auto const& pParsedAppImageStreamInfo =
      pPipelineUserConfiguration->pParsedAppImageStreamInfo;
  //
  NodeId_T const nodeId = NSCam::eNODEID_P2CaptureNode;
  //
  NodeT::InitParams initParam;
  {
    initParam.openId = pPipelineStaticInfo->sensorIds[0];
    initParam.nodeId = nodeId;
    initParam.nodeName = "P2CaptureNode";
    // according p1 node count to add open id.
    for (size_t i = 1; i < useP1NodeCount; i++) {
      initParam.subOpenIdList.push_back(pPipelineStaticInfo->sensorIds[i]);
    }
  }
  NodeT::ConfigParams cfgParam;
  {
    cfgParam.pInAppMeta = pParsedStreamInfo_NonP1->pAppMeta_Control;
    cfgParam.pInAppRetMeta = (*pParsedStreamInfo_P1)[0].pAppMeta_DynamicP1;
    cfgParam.pInHalMeta = (*pParsedStreamInfo_P1)[0].pHalMeta_DynamicP1;
    cfgParam.pOutAppMeta =
        pParsedStreamInfo_NonP1->pAppMeta_DynamicP2CaptureNode;
    cfgParam.pOutHalMeta =
        pParsedStreamInfo_NonP1->pHalMeta_DynamicP2CaptureNode;
    //
    cfgParam.pInFullRaw = (*pParsedStreamInfo_P1)[0].pHalImage_P1_Imgo;
    cfgParam.vStreamConfigure.mInStreams.push_back(
        (*pParsedStreamInfo_P1)[0].pHalImage_P1_Imgo);
    //
    cfgParam.pInResizedRaw = (*pParsedStreamInfo_P1)[0].pHalImage_P1_Rrzo;
    //
    cfgParam.pInLcsoRaw = (*pParsedStreamInfo_P1)[0].pHalImage_P1_Lcso;
    //
    // capture node not support main2 yet
    if (useP1NodeCount >
        1) {  // more than 1 p1node needs to add additional info.
      cfgParam.pInAppRetMeta2 = (*pParsedStreamInfo_P1)[1].pAppMeta_DynamicP1;
      cfgParam.pInHalMeta2 = (*pParsedStreamInfo_P1)[1].pHalMeta_DynamicP1;
      //
      cfgParam.pInFullRaw2 = (*pParsedStreamInfo_P1)[1].pHalImage_P1_Imgo;
      //
      cfgParam.pInResizedRaw2 = (*pParsedStreamInfo_P1)[1].pHalImage_P1_Rrzo;
      //
      cfgParam.pInLcsoRaw2 = (*pParsedStreamInfo_P1)[1].pHalImage_P1_Lcso;
    }
    //
    cfgParam.pInFullYuv = pParsedAppImageStreamInfo->pAppImage_Input_Yuv;
    cfgParam.vStreamConfigure.mInStreams.push_back(
        pParsedAppImageStreamInfo->pAppImage_Input_Yuv);
    //
    if (pParsedAppImageStreamInfo->pAppImage_Input_Priv) {
      cfgParam.vpInOpaqueRaws.push_back(
          pParsedAppImageStreamInfo->pAppImage_Input_Priv);
    }
    //
    if (pParsedAppImageStreamInfo->pAppImage_Output_Priv) {
      cfgParam.vpInOpaqueRaws.push_back(
          pParsedAppImageStreamInfo->pAppImage_Output_Priv);
    }
    //
    for (const auto& n : pParsedAppImageStreamInfo->vAppImage_Output_Proc) {
      cfgParam.vpOutImages.push_back(n.second);
      if (n.second->getUsageForConsumer() & GRALLOC_USAGE_HW_TEXTURE ||
          n.second->getUsageForConsumer() & GRALLOC_USAGE_HW_COMPOSER ||
          n.second->getUsageForConsumer() & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
        MY_LOGI("skip for preview/video stream");
        continue;
      }
      // 1. oapaue reprocessing add stream information to P2CaptureNode
      if (pParsedAppImageStreamInfo->pAppImage_Input_Priv &&
          pParsedAppImageStreamInfo->pAppImage_Output_Priv) {
        cfgParam.vStreamConfigure.mOutStreams.push_back(n.second);
        continue;
      }
      // 2. yuv reprocessing add stream information to P2CaptureNode
      if (pParsedAppImageStreamInfo->pAppImage_Input_Yuv) {
        cfgParam.vStreamConfigure.mOutStreams.push_back(n.second);
      }
    }
    //
    cfgParam.pOutJpegYuv = pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV;
    cfgParam.pOutThumbnailYuv =
        pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV;
    auto& ref = (*pParsedStreamInfo_P1)[0].pHalImage_P1_Imgo;
    auto pStreamInfo = std::make_shared<NSCam::v3::Utils::ImageStreamInfo>(
        "Hal:Image:Main-YUV", 0x100000000L, eSTREAMTYPE_IMAGE_OUT, 8, 2,
        ref->getUsageForConsumer(), NSCam::eImgFmt_YV12, ref->getImgSize(),
        ref->getBufPlanes(), ref->getTransform());
    cfgParam.vStreamConfigure.mOutStreams.push_back(pStreamInfo);
  }

  std::shared_ptr<NodeActorT> pNode =
      std::make_shared<NodeActorT>(NodeT::createInstance());
  pNode->setInitParam(initParam);
  pNode->setConfigParam(cfgParam);
  //
  StreamSet inStreamSet;
  StreamSet outStreamSet;
  //
  add_stream_to_set(inStreamSet, pParsedStreamInfo_NonP1->pAppMeta_Control);
  for (size_t i = 0; i < useP1NodeCount; i++) {
    add_stream_to_set(inStreamSet,
                      (*pParsedStreamInfo_P1)[i].pAppMeta_DynamicP1);
    add_stream_to_set(inStreamSet,
                      (*pParsedStreamInfo_P1)[i].pHalMeta_DynamicP1);
    add_stream_to_set(inStreamSet,
                      (*pParsedStreamInfo_P1)[i].pHalImage_P1_Imgo);
    add_stream_to_set(inStreamSet,
                      (*pParsedStreamInfo_P1)[i].pHalImage_P1_Rrzo);
    add_stream_to_set(inStreamSet,
                      (*pParsedStreamInfo_P1)[i].pHalImage_P1_Lcso);
  }
  add_stream_to_set(inStreamSet,
                    pParsedAppImageStreamInfo->pAppImage_Input_Yuv);
  add_stream_to_set(inStreamSet,
                    pParsedAppImageStreamInfo->pAppImage_Input_Priv);
  add_stream_to_set(inStreamSet,
                    pParsedAppImageStreamInfo->pAppImage_Output_Priv);
  //
  add_stream_to_set(outStreamSet,
                    pParsedStreamInfo_NonP1->pAppMeta_DynamicP2CaptureNode);
  add_stream_to_set(outStreamSet,
                    pParsedStreamInfo_NonP1->pHalMeta_DynamicP2CaptureNode);
  add_stream_to_set(outStreamSet, pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV);
  add_stream_to_set(outStreamSet,
                    pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV);
  //
  for (const auto& n : pParsedAppImageStreamInfo->vAppImage_Output_Proc) {
    add_stream_to_set(outStreamSet, n.second);
  }
  //
  NodeBuilder builder(nodeId, pNode);
  builder.addStream(NodeBuilder::eDirection_IN, inStreamSet)
      .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
  //
  setImageUsage(
      pParsedAppImageStreamInfo->pAppImage_Input_Yuv,
      NSCam::eBUFFER_USAGE_SW_READ_OFTEN | NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
  setImageUsage(
      pParsedAppImageStreamInfo->pAppImage_Input_Priv,
      NSCam::eBUFFER_USAGE_SW_READ_OFTEN | NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
  setImageUsage(
      pParsedAppImageStreamInfo->pAppImage_Output_Priv,
      NSCam::eBUFFER_USAGE_SW_READ_OFTEN | NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
  for (size_t i = 0; i < useP1NodeCount; i++) {
    setImageUsage((*pParsedStreamInfo_P1)[i].pHalImage_P1_Imgo,
                  NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                      NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage((*pParsedStreamInfo_P1)[i].pHalImage_P1_Rrzo,
                  NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                      NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
    setImageUsage((*pParsedStreamInfo_P1)[i].pHalImage_P1_Lcso,
                  NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                      NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
  }
  //
  for (const auto& n : pParsedAppImageStreamInfo->vAppImage_Output_Proc) {
    setImageUsage(n.second, NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                                NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  }
  //
  setImageUsage(pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV,
                NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                    NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  setImageUsage(pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV,
                NSCam::eBUFFER_USAGE_SW_READ_OFTEN |
                    NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  //
  int err = builder.build(pContext);
  if (err != NSCam::OK) {
    MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
  }
  return err;
}

/******************************************************************************
 *
 ******************************************************************************/
static int configContextLocked_FdNode(
    std::shared_ptr<PipelineContext> pContext,
    NSCam::v3::pipeline::model::ParsedStreamInfo_NonP1 const*
        pParsedStreamInfo_NonP1,
    uint32_t useP1NodeCount,
    InternalCommonInputParams const* pCommon) {
  typedef NSCam::v3::FdNode NodeT;
  typedef NodeActor<NodeT> NodeActorT;
  //
  NodeId_T const nodeId = NSCam::eNODEID_FDNode;
  //
  NodeT::InitParams initParam;
  {
    initParam.openId = pCommon->pPipelineStaticInfo->sensorIds[0];
    initParam.nodeId = nodeId;
    initParam.nodeName = "FDNode";
    for (size_t i = 1; i < useP1NodeCount; i++) {
      initParam.subOpenIdList.push_back(
          pCommon->pPipelineStaticInfo->sensorIds[i]);
    }
  }
  NodeT::ConfigParams cfgParam;
  {
    cfgParam.pInAppMeta = pParsedStreamInfo_NonP1->pAppMeta_Control;
    cfgParam.pInHalMeta = pParsedStreamInfo_NonP1->pHalMeta_DynamicP2StreamNode;
    cfgParam.pOutAppMeta = pParsedStreamInfo_NonP1->pAppMeta_DynamicFD;
    cfgParam.vInImage = pParsedStreamInfo_NonP1->pHalImage_FD_YUV;
  }
  //
  std::shared_ptr<NodeActorT> pNode =
      std::make_shared<NodeActorT>(NodeT::createInstance());
  pNode->setInitParam(initParam);
  pNode->setConfigParam(cfgParam);
  //
  StreamSet inStreamSet;
  StreamSet outStreamSet;
  //
  add_stream_to_set(inStreamSet, pParsedStreamInfo_NonP1->pAppMeta_Control);
  add_stream_to_set(inStreamSet,
                    pParsedStreamInfo_NonP1->pHalMeta_DynamicP2StreamNode);
  add_stream_to_set(inStreamSet, pParsedStreamInfo_NonP1->pHalImage_FD_YUV);
  //
  add_stream_to_set(outStreamSet, pParsedStreamInfo_NonP1->pAppMeta_DynamicFD);
  //
  NodeBuilder builder(nodeId, pNode);
  builder.addStream(NodeBuilder::eDirection_IN, inStreamSet)
      .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
  //
  setImageUsage(
      pParsedStreamInfo_NonP1->pHalImage_FD_YUV,
      NSCam::eBUFFER_USAGE_SW_READ_OFTEN | NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
  //
  int err = builder.build(pContext);
  if (err != NSCam::OK) {
    MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
  }
  return err;
}

/******************************************************************************
 *
 ******************************************************************************/
static int configContextLocked_JpegNode(
    std::shared_ptr<PipelineContext> pContext,
    NSCam::v3::pipeline::model::ParsedStreamInfo_NonP1 const*
        pParsedStreamInfo_NonP1,
    uint32_t useP1NodeCount,
    InternalCommonInputParams const* pCommon) {
  typedef JpegNode NodeT;
  typedef NodeActor<NodeT> NodeActorT;
  auto const& pParsedAppImageStreamInfo =
      pCommon->pPipelineUserConfiguration->pParsedAppImageStreamInfo;
  //
  NodeId_T const nodeId = NSCam::eNODEID_JpegNode;
  //
  NodeT::InitParams initParam;
  {
    initParam.openId = pCommon->pPipelineStaticInfo->sensorIds[0];
    initParam.nodeId = nodeId;
    initParam.nodeName = "JpegNode";
    for (size_t i = 1; i < useP1NodeCount; i++) {
      initParam.subOpenIdList.push_back(
          pCommon->pPipelineStaticInfo->sensorIds[i]);
    }
  }
  NodeT::ConfigParams cfgParam;
  {
    cfgParam.pInAppMeta = pParsedStreamInfo_NonP1->pAppMeta_Control;
    cfgParam.pInHalMeta_capture =
        pParsedStreamInfo_NonP1->pHalMeta_DynamicP2CaptureNode;
    cfgParam.pInHalMeta_streaming =
        pParsedStreamInfo_NonP1->pHalMeta_DynamicP2StreamNode;
    cfgParam.pOutAppMeta = pParsedStreamInfo_NonP1->pAppMeta_DynamicJpeg;
    cfgParam.pInYuv_Main = pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV;
    cfgParam.pInYuv_Thumbnail =
        pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV;
    cfgParam.pOutJpeg = pParsedAppImageStreamInfo->pAppImage_Jpeg;
  }
  //
  std::shared_ptr<NodeActorT> pNode =
      std::make_shared<NodeActorT>(NodeT::createInstance());
  pNode->setInitParam(initParam);
  pNode->setConfigParam(cfgParam);
  //
  StreamSet inStreamSet;
  StreamSet outStreamSet;
  //
  add_stream_to_set(inStreamSet, pParsedStreamInfo_NonP1->pAppMeta_Control);
  add_stream_to_set(inStreamSet,
                    pParsedStreamInfo_NonP1->pHalMeta_DynamicP2CaptureNode);
  add_stream_to_set(inStreamSet,
                    pParsedStreamInfo_NonP1->pHalMeta_DynamicP2StreamNode);
  add_stream_to_set(inStreamSet, pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV);
  add_stream_to_set(inStreamSet,
                    pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV);
  //
  add_stream_to_set(outStreamSet,
                    pParsedStreamInfo_NonP1->pAppMeta_DynamicJpeg);
  add_stream_to_set(outStreamSet, pParsedAppImageStreamInfo->pAppImage_Jpeg);
  //
  NodeBuilder builder(nodeId, pNode);
  builder.addStream(NodeBuilder::eDirection_IN, inStreamSet)
      .addStream(NodeBuilder::eDirection_OUT, outStreamSet);
  //
  setImageUsage(
      pParsedStreamInfo_NonP1->pHalImage_Jpeg_YUV,
      NSCam::eBUFFER_USAGE_SW_READ_OFTEN | NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
  setImageUsage(
      pParsedStreamInfo_NonP1->pHalImage_Thumbnail_YUV,
      NSCam::eBUFFER_USAGE_SW_READ_OFTEN | NSCam::eBUFFER_USAGE_HW_CAMERA_READ);
  setImageUsage(pParsedAppImageStreamInfo->pAppImage_Jpeg,
                NSCam::eBUFFER_USAGE_SW_WRITE_OFTEN |
                    NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE);
  //
  int err = builder.build(pContext);
  if (err != NSCam::OK) {
    MY_LOGE("build node %#" PRIxPTR " failed", nodeId);
  }
  return err;
}

/******************************************************************************
 *
 ******************************************************************************/
static int configContextLocked_Nodes(
    std::shared_ptr<PipelineContext> pContext,
    std::shared_ptr<PipelineContext> const& pOldPipelineContext,
    NSCam::v3::pipeline::model::StreamingFeatureSetting const*
        pStreamingFeatureSetting,
    NSCam::v3::pipeline::model::CaptureFeatureSetting const*
        pCaptureFeatureSetting,
    std::vector<NSCam::v3::pipeline::model::ParsedStreamInfo_P1> const*
        pParsedStreamInfo_P1,
    NSCam::v3::pipeline::model::ParsedStreamInfo_NonP1 const*
        pParsedStreamInfo_NonP1,
    NSCam::v3::pipeline::model::PipelineNodesNeed const* pPipelineNodesNeed,
    std::vector<NSCam::v3::pipeline::model::SensorSetting> const*
        pSensorSetting,
    std::vector<NSCam::v3::pipeline::policy::P1HwSetting> const* pvP1HwSetting,
    uint32_t batchSize,
    InternalCommonInputParams const* pCommon) {
  CAM_TRACE_CALL();

  auto const& pPipelineStaticInfo = pCommon->pPipelineStaticInfo;

  uint32_t useP1NodeCount = 0;
  for (const auto n : pPipelineNodesNeed->needP1Node) {
    if (n) {
      useP1NodeCount++;
    }
  }

  // if useP1NodeCount more than 1, it has to create sync helper
  // and assign to p1 config param.
  if (useP1NodeCount > 1) {
    MY_LOGW("useP1NodeCount more than one");
  }

  for (size_t i = 0; i < pPipelineNodesNeed->needP1Node.size(); i++) {
    if (pPipelineNodesNeed->needP1Node[i]) {
      CHECK_ERROR(configContextLocked_P1Node(
                      pContext, pOldPipelineContext, pStreamingFeatureSetting,
                      &(*pParsedStreamInfo_P1)[i], pParsedStreamInfo_NonP1,
                      &(*pSensorSetting)[i], &(*pvP1HwSetting)[i], i, batchSize,
                      useP1NodeCount > 1, pCommon),
                  "\nconfigContextLocked_P1Node");
    }
  }
  if (pPipelineNodesNeed->needP2StreamNode) {
    bool hasMonoSensor = false;
    for (auto const v : pPipelineStaticInfo->sensorRawTypes) {
      if (NSCam::SENSOR_RAW_MONO == v) {
        hasMonoSensor = true;
        break;
      }
    }
    CHECK_ERROR(configContextLocked_P2SNode(
                    pContext, pStreamingFeatureSetting, pParsedStreamInfo_P1,
                    pParsedStreamInfo_NonP1, batchSize, useP1NodeCount,
                    hasMonoSensor, pCommon),
                "\nconfigContextLocked_P2SNode");
  }
  if (pPipelineNodesNeed->needP2CaptureNode) {
    CHECK_ERROR(configContextLocked_P2CNode(
                    pContext, pCaptureFeatureSetting, pParsedStreamInfo_P1,
                    pParsedStreamInfo_NonP1, useP1NodeCount, pCommon),
                "\nconfigContextLocked_P2CNode");
  }
  if (pPipelineNodesNeed->needFDNode) {
    CHECK_ERROR(configContextLocked_FdNode(pContext, pParsedStreamInfo_NonP1,
                                           useP1NodeCount, pCommon),
                "\nconfigContextLocked_FdNode");
  }
  if (pPipelineNodesNeed->needJpegNode) {
    CHECK_ERROR(configContextLocked_JpegNode(pContext, pParsedStreamInfo_NonP1,
                                             useP1NodeCount, pCommon),
                "\nconfigContextLocked_JpegNode");
  }
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
static int configContextLocked_Pipeline(
    std::shared_ptr<PipelineContext> pContext,
    NSCam::v3::pipeline::model::PipelineNodesNeed const* pPipelineNodesNeed) {
  CAM_TRACE_CALL();
  NodeSet roots;
  {
    roots.add(NSCam::eNODEID_P1Node);
    if (pPipelineNodesNeed->needP1Node.size() > 1) {
      roots.add(NSCam::eNODEID_P1Node_main2);
    }
  }
  NodeEdgeSet edges;
  {
    // in:p1 -> out:
    if (pPipelineNodesNeed->needP1Node.size() > 0) {
      if (pPipelineNodesNeed->needP1Node[0]) {
        NodeId_T id = NSCam::eNODEID_P1Node;
        if (pPipelineNodesNeed->needP2StreamNode) {
          edges.addEdge(id, NSCam::eNODEID_P2StreamNode);
        }
        if (pPipelineNodesNeed->needP2CaptureNode) {
          edges.addEdge(id, NSCam::eNODEID_P2CaptureNode);
        }
      }
    }
    // in:p2 -> out:
    if (pPipelineNodesNeed->needP2StreamNode) {
      if (pPipelineNodesNeed->needFDNode) {
        edges.addEdge(NSCam::eNODEID_P2StreamNode, NSCam::eNODEID_FDNode);
      }
      if (pPipelineNodesNeed->needJpegNode) {
        edges.addEdge(NSCam::eNODEID_P2StreamNode, NSCam::eNODEID_JpegNode);
      }
    }
    // in:p2ts -> out:
    if (pPipelineNodesNeed->needP2CaptureNode) {
      if (pPipelineNodesNeed->needJpegNode) {
        edges.addEdge(NSCam::eNODEID_P2CaptureNode, NSCam::eNODEID_JpegNode);
      }
    }
  }
  //
  CHECK_ERROR(
      PipelineBuilder().setRootNode(roots).setNodeEdges(edges).build(pContext),
      "\nPipelineBuilder.build(pContext)");
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {
auto buildPipelineContext(std::shared_ptr<PipelineContext>* out,
                          BuildPipelineContextInputParams const& in) -> int {
  InternalCommonInputParams const common{
      .pPipelineStaticInfo = in.pPipelineStaticInfo,
      .pPipelineUserConfiguration = in.pPipelineUserConfiguration,
      .bIsReConfigure = in.bIsReconfigure,
  };

  //
  if (in.pOldPipelineContext) {
    MY_LOGD("old PipelineContext - getStrongCount:%ld",
            in.pOldPipelineContext.use_count());
    CHECK_ERROR(in.pOldPipelineContext->waitUntilNodeDrained(0x01),
                "\npipelineContext->waitUntilNodeDrained");
  }

  //
  std::shared_ptr<PipelineContext> pNewPipelineContext =
      PipelineContext::create(in.pipelineName.c_str());
  CHECK_ERROR(pNewPipelineContext->beginConfigure(in.pOldPipelineContext),
              "\npipelineContext->beginConfigure");

  // config Streams
  CHECK_ERROR(
      configContextLocked_Streams(pNewPipelineContext, in.pvParsedStreamInfo_P1,
                                  in.pZSLProvider, in.pParsedStreamInfo_NonP1,
                                  in.pPipelineUserConfiguration),
      "\nconfigContextLocked_Streams");

  // config Nodes
  CHECK_ERROR(configContextLocked_Nodes(
                  pNewPipelineContext, in.pOldPipelineContext,
                  in.pStreamingFeatureSetting, in.pCaptureFeatureSetting,
                  in.pvParsedStreamInfo_P1, in.pParsedStreamInfo_NonP1,
                  in.pPipelineNodesNeed, in.pvSensorSetting, in.pvP1HwSetting,
                  in.batchSize, &common),
              "\nconfigContextLocked_Nodes");

  // config pipeline
  CHECK_ERROR(
      configContextLocked_Pipeline(pNewPipelineContext, in.pPipelineNodesNeed),
      "\npipelineContext->configContextLocked_Pipeline");
  //
  CHECK_ERROR(pNewPipelineContext->setDataCallback(in.pDataCallback),
              "\npipelineContext->setDataCallback");
  //
  CHECK_ERROR(pNewPipelineContext->endConfigure(
                  in.bUsingMultiThreadToBuildPipelineContext),
              "\npipelineContext->endConfigure");

  *out = pNewPipelineContext;
  return NSCam::OK;
}
};  // namespace model
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
