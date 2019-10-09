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

#define LOG_TAG "mtkcam-ConfigStreamInfoPolicy"

#include "mtkcam/pipeline/policy/config/ConfigStreamInfoPolicy.h"
#include <mtkcam/pipeline/policy/IConfigStreamInfoPolicy.h>
//
#include <array>
#include <compiler.h>
#include <memory>
//
#include <mtkcam/pipeline/hwnode/StreamId.h>
#include <mtkcam/aaa/IIspMgr.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>
//
#include "MyUtils.h"
#include <string>
#include <utility>

/******************************************************************************
 *
 ******************************************************************************/
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

/******************************************************************************
 *
 ******************************************************************************/
static std::string getP1StreamNamePostfix(size_t i) {
  switch (i) {
    case 0:
      return std::string("main1");
    case 1:
      return std::string("main2");
    default:
      break;
  }
  return std::string("na");
}

/******************************************************************************
 *
 ******************************************************************************/
static constexpr StreamId_T getP1StreamId(size_t i,
                                          std::array<StreamId_T, 2> const& a) {
  if (CC_UNLIKELY(i >= a.size())) {
    MY_LOGE("not support p1 node number large than 2");
    return eSTREAMID_NO_STREAM;
  }
  return a[i];
}

#define GETP1STREAMID(_type_, ...)                             \
  static constexpr StreamId_T getStreamId_##_type_(size_t i) { \
    return getP1StreamId(i, {__VA_ARGS__});                    \
  }

GETP1STREAMID(P1AppMetaDynamic,
              eSTREAMID_META_APP_DYNAMIC_01,
              eSTREAMID_META_APP_DYNAMIC_01_MAIN2)

GETP1STREAMID(P1HalMetaDynamic,
              eSTREAMID_META_PIPE_DYNAMIC_01,
              eSTREAMID_META_PIPE_DYNAMIC_01_MAIN2)

GETP1STREAMID(P1HalMetaControl,
              eSTREAMID_META_PIPE_CONTROL,
              eSTREAMID_META_PIPE_CONTROL_MAIN2)

GETP1STREAMID(P1Imgo,
              eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00,
              eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01)

GETP1STREAMID(P1Rrzo,
              eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00,
              eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01)

GETP1STREAMID(P1Lcso,
              eSTREAMID_IMAGE_PIPE_RAW_LCSO_00,
              eSTREAMID_IMAGE_PIPE_RAW_LCSO_01)

GETP1STREAMID(P1Rsso,
              eSTREAMID_IMAGE_PIPE_RAW_RSSO_00,
              eSTREAMID_IMAGE_PIPE_RAW_RSSO_01)

/******************************************************************************
 *
 ******************************************************************************/
auto P1MetaStreamInfoBuilder::setP1AppMetaDynamic_Default()
    -> P1MetaStreamInfoBuilder& {
  setStreamName(std::string("App:Meta:DynamicP1_") +
                getP1StreamNamePostfix(mIndex));
  setStreamId(getStreamId_P1AppMetaDynamic(mIndex));
  setStreamType(eSTREAMTYPE_META_OUT);
  setMaxBufNum(10);
  setMinInitBufNum(1);

  return *this;
}

auto P1MetaStreamInfoBuilder::setP1HalMetaDynamic_Default()
    -> P1MetaStreamInfoBuilder& {
  setStreamName(std::string("Hal:Meta:P1:Dynamic_") +
                getP1StreamNamePostfix(mIndex));
  setStreamId(getStreamId_P1HalMetaDynamic(mIndex));
  setStreamType(eSTREAMTYPE_META_INOUT);
  setMaxBufNum(10);
  setMinInitBufNum(1);

  return *this;
}

auto P1MetaStreamInfoBuilder::setP1HalMetaControl_Default()
    -> P1MetaStreamInfoBuilder& {
  setStreamName(std::string("Hal:Meta:Control_") +
                getP1StreamNamePostfix(mIndex));
  setStreamId(getStreamId_P1HalMetaControl(mIndex));
  setStreamType(eSTREAMTYPE_META_IN);
  setMaxBufNum(10);
  setMinInitBufNum(1);

  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
auto P1ImageStreamInfoBuilder::setP1Imgo_Default(
    size_t maxBufNum, P1HwSetting const& rP1HwSetting)
    -> P1ImageStreamInfoBuilder& {
  MINT const imgFormat = rP1HwSetting.imgoFormat;
  MSize const& imgSize = rP1HwSetting.imgoSize;
  size_t const stride = rP1HwSetting.imgoStride;

  setStreamName(
      (std::string("Hal:Image:P1:Fullraw_") + getP1StreamNamePostfix(mIndex)));
  setStreamId(getStreamId_P1Imgo(mIndex));
  setStreamType(eSTREAMTYPE_IMAGE_INOUT);
  setMaxBufNum(maxBufNum);
  setMinInitBufNum(0);
  setUsageForAllocator(0);
  setImgFormat(imgFormat);
  setImgSize(imgSize);
  setBufPlanes(toBufPlanes(stride, imgFormat, imgSize));

  return *this;
}

auto P1ImageStreamInfoBuilder::setP1Rrzo_Default(
    size_t maxBufNum, P1HwSetting const& rP1HwSetting)
    -> P1ImageStreamInfoBuilder& {
  MINT const imgFormat = rP1HwSetting.rrzoFormat;
  MSize const& imgSize = rP1HwSetting.rrzoSize;
  size_t const stride = rP1HwSetting.rrzoStride;
  setStreamName((std::string("Hal:Image:P1:Resizeraw_") +
                 getP1StreamNamePostfix(mIndex)));
  setStreamId(getStreamId_P1Rrzo(mIndex));
  setStreamType(eSTREAMTYPE_IMAGE_INOUT);
  setMaxBufNum(maxBufNum);
  setMinInitBufNum(0);
  setUsageForAllocator(0);
  setImgFormat(imgFormat);
  setImgSize(imgSize);
  setBufPlanes(toBufPlanes(stride, imgFormat, imgSize));

  return *this;
}

auto P1ImageStreamInfoBuilder::setP1Lcso_Default(size_t maxBufNum)
    -> P1ImageStreamInfoBuilder& {
  // p1: lcso size
  MUINT const usage = 0;
  NS3Av3::LCSO_Param lcsoParam;

#if (MTKCAM_ENABLE_IPC == 1)
  if (auto ispMgr = MAKE_IspMgr("ConfigStreamInfoPolicy")) {
    ispMgr->queryLCSOParams(&lcsoParam);
    ispMgr->uninit("ConfigStreamInfoPolicy");
  } else {
    MY_LOGE("Query IIspMgr FAILED!");
  }

#else
  if (auto ispMgr = MAKE_IspMgr()) {
    ispMgr->queryLCSOParams(&lcsoParam);
  } else {
    MY_LOGE("Query IIspMgr FAILED!");
  }
#endif
  // metadata buffer from P1 driver
  MBOOL bEnable = ::property_get_int32("vendor.debug.enable.lcs", 1);
  if (bEnable) {
    lcsoParam.size.w = lcsoParam.size.w * lcsoParam.size.h * 2;
    lcsoParam.size.h = 1;
    lcsoParam.stride = lcsoParam.size.w;
  }
  setStreamName(
      (std::string("Hal:Image:LCSraw_") + getP1StreamNamePostfix(mIndex)));
  setStreamId(getStreamId_P1Lcso(mIndex));
  setStreamType(eSTREAMTYPE_IMAGE_INOUT);
  setMaxBufNum(maxBufNum);
  setMinInitBufNum(1);
  setUsageForAllocator(usage);
  setImgFormat(bEnable ? eImgFmt_BLOB : lcsoParam.format);
  setImgSize(lcsoParam.size);
  setBufPlanes(toBufPlanes(lcsoParam.stride, lcsoParam.format, lcsoParam.size));
  return *this;
}

auto P1ImageStreamInfoBuilder::setP1Rsso_Default(
    size_t maxBufNum, P1HwSetting const& rP1HwSetting)
    -> P1ImageStreamInfoBuilder& {
  MINT imgFormat = eImgFmt_STA_BYTE;
  MSize const& imgSize = rP1HwSetting.rssoSize;
  size_t const stride = imgSize.w;

  setStreamName(
      (std::string("Hal:Image:RSSO_") + getP1StreamNamePostfix(mIndex)));
  setStreamId(getStreamId_P1Rsso(mIndex));
  setStreamType(eSTREAMTYPE_IMAGE_INOUT);
  setMaxBufNum(maxBufNum);
  setMinInitBufNum(1);
  setUsageForAllocator(eBUFFER_USAGE_SW_READ_OFTEN |
                       eBUFFER_USAGE_HW_CAMERA_READWRITE);
  setImgFormat(eImgFmt_STA_BYTE);
  setImgSize(imgSize);
  setBufPlanes(toBufPlanes(stride, imgFormat, imgSize));

  return *this;
}

auto P1ImageStreamInfoBuilder::toBufPlanes(size_t stride,
                                           MINT imgFormat,
                                           MSize const& imgSize)
    -> IImageStreamInfo::BufPlanes_t {
  IImageStreamInfo::BufPlanes_t bufPlanes;
  //
#define addBufPlane(planes, height, stride)                             \
  do {                                                                  \
    size_t _height = (size_t)(height);                                  \
    size_t _stride = (size_t)(stride);                                  \
    IImageStreamInfo::BufPlane bufPlane = {_height * _stride, _stride}; \
    planes.push_back(bufPlane);                                         \
  } while (0)
  switch (imgFormat) {
    case eImgFmt_BAYER10:
    case eImgFmt_FG_BAYER10:
    case eImgFmt_BAYER8:  // LCSO
    case eImgFmt_STA_BYTE:
    case eImgFmt_STA_2BYTE:  // LCSO with LCE3.0
      addBufPlane(bufPlanes, imgSize.h, stride);
      break;
    case eImgFmt_UFO_BAYER8:
    case eImgFmt_UFO_BAYER10:
    case eImgFmt_UFO_BAYER12:
    case eImgFmt_UFO_BAYER14:
    case eImgFmt_UFO_FG_BAYER8:
    case eImgFmt_UFO_FG_BAYER10:
    case eImgFmt_UFO_FG_BAYER12:
    case eImgFmt_UFO_FG_BAYER14: {
      size_t ufoStride[3] = {0};
      addBufPlane(bufPlanes, imgSize.h, stride);
      mHwInfoHelper->queryUFOStride(imgFormat, imgSize, ufoStride);
      addBufPlane(bufPlanes, imgSize.h, ufoStride[1]);
      addBufPlane(bufPlanes, imgSize.h, ufoStride[2]);
      break;
    }
    default:
      MY_LOGE("format not support yet %d", imgFormat);
      break;
  }
#undef addBufPlane
  return bufPlanes;
}

/******************************************************************************
 *
 ******************************************************************************/
static std::shared_ptr<NSCam::v3::Utils::ImageStreamInfo> createImageStreamInfo(
    char const* streamName,
    StreamId_T streamId,
    MUINT32 streamType,
    size_t maxBufNum,
    size_t minInitBufNum,
    MUINT usageForAllocator,
    MINT imgFormat,
    MSize const& imgSize,
    MUINT32 transform = 0) {
  IImageStreamInfo::BufPlanes_t bufPlanes;
#define addBufPlane(planes, height, stride)                             \
  do {                                                                  \
    size_t _height = (size_t)(height);                                  \
    size_t _stride = (size_t)(stride);                                  \
    IImageStreamInfo::BufPlane bufPlane = {_height * _stride, _stride}; \
    planes.push_back(bufPlane);                                         \
  } while (0)
  switch (imgFormat) {
    case eImgFmt_YV12:
      addBufPlane(bufPlanes, imgSize.h, imgSize.w);
      addBufPlane(bufPlanes, imgSize.h >> 1, imgSize.w >> 1);
      addBufPlane(bufPlanes, imgSize.h >> 1, imgSize.w >> 1);
      break;
    case eImgFmt_NV21:
    case eImgFmt_NV12:
      addBufPlane(bufPlanes, imgSize.h, imgSize.w);
      addBufPlane(bufPlanes, imgSize.h >> 1, imgSize.w);
      break;
    case eImgFmt_YUY2:
      addBufPlane(bufPlanes, imgSize.h, imgSize.w << 1);
      break;
    default:
      MY_LOGE("format not support yet %d", imgFormat);
      break;
  }
#undef addBufPlane

  auto pStreamInfo = std::make_shared<NSCam::v3::Utils::ImageStreamInfo>(
      streamName, streamId, streamType, maxBufNum, minInitBufNum,
      usageForAllocator, imgFormat, imgSize, bufPlanes, transform);

  if (pStreamInfo == nullptr) {
    MY_LOGE("create ImageStream failed, %s, %#" PRIx64, streamName, streamId);
  }

  return pStreamInfo;
}

/******************************************************************************
 *
 ******************************************************************************/
static MVOID evaluatePreviewSize(
    PipelineUserConfiguration const* pPipelineUserConfiguration, MSize* rSize) {
  std::shared_ptr<IImageStreamInfo> pStreamInfo;
  int consumer_usage = 0;
  int allocate_usage = 0;
  int maxheight = rSize->h;
  int prevwidth = 0;
  int prevheight = 0;

  for (const auto& n : pPipelineUserConfiguration->pParsedAppImageStreamInfo
                           ->vAppImage_Output_Proc) {
    if ((pStreamInfo = n.second) != 0) {
      consumer_usage = pStreamInfo->getUsageForConsumer();
      allocate_usage = pStreamInfo->getUsageForAllocator();
      MY_LOGD("consumer : %X, allocate : %X", consumer_usage, allocate_usage);
      if (consumer_usage & GRALLOC_USAGE_HW_TEXTURE) {
        prevwidth = pStreamInfo->getImgSize().w;
        prevheight = pStreamInfo->getImgSize().h;
        break;
      }
      if (consumer_usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
        continue;
      }
      prevwidth = pStreamInfo->getImgSize().w;
      prevheight = pStreamInfo->getImgSize().h;
    }
  }
  if (prevwidth == 0 || prevheight == 0) {
    return;
  }
  rSize->h = prevheight * rSize->w / prevwidth;
  if (maxheight < rSize->h) {
    MY_LOGW(
        "Warning!!,  scaled preview height(%d) is larger than max height(%d)",
        rSize->h, maxheight);
    rSize->h = maxheight;
  }
  MY_LOGD("evaluate preview size : %dx%d", prevwidth, prevheight);
  MY_LOGD("FD buffer size : %dx%d", rSize->w, rSize->h);
}

/**
 * default version
 */
FunctionType_Configuration_StreamInfo_P1
makePolicy_Configuration_StreamInfo_P1_Default() {
  return [](Configuration_StreamInfo_P1_Params const& params) -> int {
    auto pvOut = params.pvOut;
    auto pvP1HwSetting = params.pvP1HwSetting;
    auto pvP1DmaNeed = params.pvP1DmaNeed;
    auto pPipelineNodesNeed = params.pPipelineNodesNeed;
    auto pPipelineStaticInfo = params.pPipelineStaticInfo;

    for (size_t i = 0; i < pPipelineNodesNeed->needP1Node.size(); i++) {
      ParsedStreamInfo_P1 config;
      if (pPipelineNodesNeed->needP1Node[i]) {
        // MetaData
        {
          P1MetaStreamInfoBuilder builder(i);
          builder.setP1AppMetaDynamic_Default();
          config.pAppMeta_DynamicP1 = builder.build();
        }
        {
          P1MetaStreamInfoBuilder builder(i);
          builder.setP1HalMetaDynamic_Default();
          config.pHalMeta_DynamicP1 = builder.build();
        }
        {
          P1MetaStreamInfoBuilder builder(i);
          builder.setP1HalMetaControl_Default();
          config.pHalMeta_Control = builder.build();
        }

        std::shared_ptr<NSCamHW::HwInfoHelper> infohelper =
            std::make_shared<NSCamHW::HwInfoHelper>(
                pPipelineStaticInfo->sensorIds[i]);
        if (!(infohelper != nullptr && infohelper->updateInfos())) {
          MY_LOGE("HwInfoHelper");
          return NO_INIT;
        }

        MINT32 p1_out_buffer_size = 8;
        int is_low_mem = property_get_int32("ro.config.low_ram", false);
        if (is_low_mem) {
          p1_out_buffer_size =
              6;  // For low memory project, set buffer size to 6
        }
        MINT32 P1_streamBufCnt = 10;  // ::property_get_int32(
                                      // "vendor.debug.camera.p1.streamBufCnt",
                                      // p1_out_buffer_size);
        MY_LOGD("P1 out image buffer size = %d", P1_streamBufCnt);
        // IMGO
        if ((*pvP1DmaNeed)[i] & P1_IMGO) {
          P1ImageStreamInfoBuilder builder(i, infohelper);
          builder.setP1Imgo_Default(P1_streamBufCnt, (*pvP1HwSetting)[i]);
          config.pHalImage_P1_Imgo = builder.build();
        }
        // RRZO
        if ((*pvP1DmaNeed)[i] & P1_RRZO) {
          P1ImageStreamInfoBuilder builder(i, infohelper);
          builder.setP1Rrzo_Default(P1_streamBufCnt, (*pvP1HwSetting)[i]);
          config.pHalImage_P1_Rrzo = builder.build();
        }
        // LCSO
        if ((*pvP1DmaNeed)[i] & P1_LCSO) {
          P1ImageStreamInfoBuilder builder(i, infohelper);
          builder.setP1Lcso_Default(P1_streamBufCnt);
          config.pHalImage_P1_Lcso = builder.build();
        }
        // RSSO
        if ((*pvP1DmaNeed)[i] & P1_RSSO) {
          P1ImageStreamInfoBuilder builder(i, infohelper);
          builder.setP1Rsso_Default(P1_streamBufCnt, (*pvP1HwSetting)[i]);
          config.pHalImage_P1_Rsso = builder.build();
        }
      }
      pvOut->push_back(std::move(config));
    }

    return OK;
  };
}

/**
 * default version
 */
FunctionType_Configuration_StreamInfo_NonP1
makePolicy_Configuration_StreamInfo_NonP1_Default() {
  return [](Configuration_StreamInfo_NonP1_Params const& params) -> int {
    auto pOut = params.pOut;
    auto pPipelineNodesNeed = params.pPipelineNodesNeed;
    auto pCaptureFeatureSetting = params.pCaptureFeatureSetting;
    auto pPipelineUserConfiguration = params.pPipelineUserConfiguration;
    auto const& pParsedAppImageStreamInfo =
        pPipelineUserConfiguration->pParsedAppImageStreamInfo;

    pOut->pAppMeta_Control =
        pPipelineUserConfiguration->vMetaStreams.begin()->second;

    if (pPipelineNodesNeed->needP2StreamNode) {
      pOut->pAppMeta_DynamicP2StreamNode =
          std::make_shared<NSCam::v3::Utils::MetaStreamInfo>(
              "App:Meta:DynamicP2", eSTREAMID_META_APP_DYNAMIC_02,
              eSTREAMTYPE_META_OUT, 10, 1);
      pOut->pHalMeta_DynamicP2StreamNode =
          std::make_shared<NSCam::v3::Utils::MetaStreamInfo>(
              "Hal:Meta:P2:Dynamic", eSTREAMID_META_PIPE_DYNAMIC_02,
              eSTREAMTYPE_META_INOUT, 10, 1);
    }
    if (pPipelineNodesNeed->needP2CaptureNode) {
      pOut->pAppMeta_DynamicP2CaptureNode =
          std::make_shared<NSCam::v3::Utils::MetaStreamInfo>(
              "App:Meta:DynamicP2Cap", eSTREAMID_META_APP_DYNAMIC_02_CAP,
              eSTREAMTYPE_META_OUT, 10, 1);
      pOut->pHalMeta_DynamicP2CaptureNode =
          std::make_shared<NSCam::v3::Utils::MetaStreamInfo>(
              "Hal:Meta:P2C:Dynamic", eSTREAMID_META_PIPE_DYNAMIC_02_CAP,
              eSTREAMTYPE_META_INOUT, 10, 1);
    }
    if (pPipelineNodesNeed->needFDNode) {
      pOut->pAppMeta_DynamicFD =
          std::make_shared<NSCam::v3::Utils::MetaStreamInfo>(
              "App:Meta:FD", eSTREAMID_META_APP_DYNAMIC_FD,
              eSTREAMTYPE_META_OUT, 10, 1);
      // FD YUV
      // MSize const size(640, 480); //FIXME: hard-code here?
      MSize size(640, 480);
      // evaluate preview size
      evaluatePreviewSize(pPipelineUserConfiguration, &size);

      MY_LOGD("evaluate FD buffer size : %dx%d", size.w, size.h);

      MINT const format = eImgFmt_YUY2;  // eImgFmt_YV12;
      MUINT const usage = 0;

      pOut->pHalImage_FD_YUV = createImageStreamInfo(
          "Hal:Image:FD", eSTREAMID_IMAGE_FD, eSTREAMTYPE_IMAGE_INOUT, 5, 1,
          usage, format, size);
    }
    if (pPipelineNodesNeed->needJpegNode) {
      pOut->pAppMeta_DynamicJpeg =
          std::make_shared<NSCam::v3::Utils::MetaStreamInfo>(
              "App:Meta:Jpeg", eSTREAMID_META_APP_DYNAMIC_JPEG,
              eSTREAMTYPE_META_OUT, 10, 1);

      uint32_t const maxJpegNum =
          (pCaptureFeatureSetting) ? pCaptureFeatureSetting->maxAppJpegStreamNum
                                   : 1;
      MUINT32 transform =
          pParsedAppImageStreamInfo->pAppImage_Jpeg->getTransform();
      // Jpeg YUV
      {
        MSize const& size =
            pParsedAppImageStreamInfo->pAppImage_Jpeg->getImgSize();
        MINT const format = eImgFmt_NV12;
        MUINT const usage = 0;
        pOut->pHalImage_Jpeg_YUV = createImageStreamInfo(
            "Hal:Image:YuvJpeg", eSTREAMID_IMAGE_PIPE_YUV_JPEG_00,
            eSTREAMTYPE_IMAGE_INOUT, maxJpegNum, 0, usage, format, size,
            transform);
      }
      // Thumbnail YUV
      {
        MINT openId = params.pPipelineStaticInfo->openId;
        std::shared_ptr<IMetadataProvider> pMetadataProvider =
            NSMetadataProviderManager::valueFor(openId);
        IMetadata static_meta =
            pMetadataProvider->getMtkStaticCharacteristics();
        IMetadata::IEntry const& entryAvaliableSize =
            static_meta.entryFor(MTK_JPEG_AVAILABLE_THUMBNAIL_SIZES);
        if (entryAvaliableSize.count() == 0) {
          MY_LOGW("No tag: MTK_JPEG_AVAILABLE_THUMBNAIL_SIZES");
        }
        MSize const size = entryAvaliableSize.itemAt(
            (entryAvaliableSize.count() - 1), Type2Type<MSize>());
        MINT const format = eImgFmt_YUY2;
        MUINT const usage = 0;
        pOut->pHalImage_Thumbnail_YUV = createImageStreamInfo(
            "Hal:Image:YuvThumbnail", eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL_00,
            eSTREAMTYPE_IMAGE_INOUT, maxJpegNum, 0, usage, format, size,
            transform);
      }
    }

    return OK;
  };
}

};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
