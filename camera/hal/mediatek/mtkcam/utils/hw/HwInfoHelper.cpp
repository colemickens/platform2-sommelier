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

#define LOG_TAG "MtkCam/HwInfoHelper"
//
#include <mtkcam/drv/def/ispio_port_index.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/drv/iopipe/CamIO/Cam_QueryDef.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IHalCamIO.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IIOPipe.h>
#include <mtkcam/utils/hw/HwInfoHelper.h>
#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/std/Log.h>
#include <property_service/property.h>
#include <property_service/property_lib.h>
//
#include <string>
#include <vector>

using NSCam::IHalSensor;
using NSCam::IHalSensorList;
using NSCam::SensorStaticInfo;
using NSCam::NSIoPipe::PORT_IMGO;
using NSCam::NSIoPipe::PORT_RRZO;
using NSCam::NSIoPipe::NSCamIOPipe::CAM_Pipeline_12BITS;
using NSCam::NSIoPipe::NSCamIOPipe::CAM_Pipeline_14BITS;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_ISP_RES;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_PIPELINE_BITDEPTH;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_QUERY_FMT;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE;
using NSCam::NSIoPipe::NSCamIOPipe::IV4L2PipeFactory;
using NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryIn;
using NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo;
using NSCam::NSIoPipe::NSCamIOPipe::sCAM_QUERY_BURST_NUM;
using NSCam::NSIoPipe::NSCamIOPipe::sCAM_QUERY_ISP_RES;
using NSCam::NSIoPipe::NSCamIOPipe::sCAM_QUERY_SUPPORT_PATTERN;
using NSCamHW::HwInfoHelper;
/******************************************************************************
 *
 ******************************************************************************/
class HwInfoHelper::Implementor {
 protected:
  MINT32 const mOpenId;
  SensorStaticInfo mSensorStaticInfo;
  MINT32 mUseUFO;
  MINT32 mUseUFO_IMGO;
  MINT32 mUseUFO_RRZO;

 public:
  explicit Implementor(MINT32 const openId);
  virtual ~Implementor() {}

 public:
  MBOOL updateInfos();
  MBOOL isRaw() const {
    return mSensorStaticInfo.sensorType == NSCam::SENSOR_TYPE_RAW;
  }
  MBOOL isYuv() const {
    return mSensorStaticInfo.sensorType == NSCam::SENSOR_TYPE_YUV;
  }
  MBOOL getSensorSize(MUINT32 const sensorMode, NSCam::MSize* size) const;
  MBOOL getSensorFps(MUINT32 const sensorMode, MINT32* fps) const;
  MBOOL getImgoFmt(MUINT32 const bitDepth,
                   MINT* fmt,
                   MBOOL forceUFO = MFALSE,
                   MBOOL useUnpakFmt = MFALSE) const;
  MBOOL getRrzoFmt(MUINT32 const bitDepth,
                   MINT* fmt,
                   MBOOL forceUFO = MFALSE) const;
  MBOOL getLpModeSupportBitDepthFormat(const MINT& fmt, MUINT32* pipeBit) const;
  MBOOL getRecommendRawBitDepth(MINT32* bitDepth) const;
  MBOOL getSensorPowerOnPredictionResult(MBOOL* isPowerOnSuccess) const;
  //
  MBOOL queryPixelMode(MUINT32 const sensorMode,
                       MINT32 const fps,
                       MUINT32* pixelMode) const;
  virtual MBOOL alignPass1HwLimitation(MUINT32 const pixelMode,
                                       MINT const imgFormat,
                                       MBOOL isImgo,
                                       NSCam::MSize* size,
                                       size_t* stride) const;
  virtual MBOOL alignRrzoHwLimitation(NSCam::MSize const targetSize,
                                      NSCam::MSize const sensorSize,
                                      NSCam::MSize* result) const;
  MBOOL querySupportVHDRMode(MUINT32 const sensorMode, MUINT32* vhdrMode) const;
  MBOOL quertMaxRrzoWidth(MINT32* maxWidth) const;
  MBOOL getSensorRawFmtType(MUINT32* u4RawFmtType) const;
  MBOOL queryUFOStride(MINT const imgFormat,
                       MSize const imgSize,
                       size_t* stride) const;
  MBOOL getShutterDelayFrameCount(MINT32* shutterDelayCnt) const;
  MBOOL shrinkCropRegion(NSCam::MSize const sensorSize,
                         NSCam::MRect* cropRegion,
                         MINT32 shrinkPx = 2) const;

  MBOOL querySupportResizeRatio(MUINT32* rPrecentage) const;
  MBOOL querySupportBurstNum(MUINT32* rBitField) const;
  MBOOL querySupportRawPattern(MUINT32* rBitField) const;

  static MBOOL getDynamicTwinSupported();
  static MUINT32 getCameraSensorPowerOnCount();
};

/******************************************************************************
 *
 ******************************************************************************/
HwInfoHelper::Implementor::Implementor(MINT32 const openId)
    : mOpenId(openId)
#ifdef USE_UFO
      ,
      mUseUFO(1),
      mUseUFO_IMGO(1),
      mUseUFO_RRZO(1)
#else
      ,
      mUseUFO(0),
      mUseUFO_IMGO(0),
      mUseUFO_RRZO(0)
#endif
{
  ::memset(&mSensorStaticInfo, 0, sizeof(SensorStaticInfo));
  MINT disableUFO = ::property_get_int32("vendor.debug.camera.ufo_off", 0);
  MINT disableUFO_IMGO =
      ::property_get_int32("vendor.debug.camera.ufo_off.imgo", 0);
  MINT disableUFO_RRZO =
      ::property_get_int32("vendor.debug.camera.ufo_off.rrzo", 1);
  if (disableUFO) {
    mUseUFO = 0;
    mUseUFO_IMGO = 0;
    mUseUFO_RRZO = 0;
  } else {
    if (disableUFO_IMGO) {
      mUseUFO_IMGO = 0;
    }
    if (disableUFO_RRZO) {
      mUseUFO_RRZO = 0;
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::updateInfos() {
  return mpImp->updateInfos();
}

MBOOL
HwInfoHelper::Implementor::updateInfos() {
  IHalSensorList* pSensorList = GET_HalSensorList();
  if (!pSensorList) {
    MY_LOGE("cannot get sensorlist");
    return MFALSE;
  }
  MUINT32 sensorDev = pSensorList->querySensorDevIdx(mOpenId);
  pSensorList->querySensorStaticInfo(sensorDev, &mSensorStaticInfo);
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::isRaw() const {
  return mpImp->isRaw();
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::isYuv() const {
  return mpImp->isYuv();
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::getSensorSize(MUINT32 const sensorMode, MSize* size) const {
  return mpImp->getSensorSize(sensorMode, size);
}

MBOOL
HwInfoHelper::Implementor::getSensorSize(MUINT32 const sensorMode,
                                         MSize* size) const {
  switch (sensorMode) {
#define scenario_case(scenario, KEY, size_var)   \
  case scenario:                                 \
    size_var->w = mSensorStaticInfo.KEY##Width;  \
    size_var->h = mSensorStaticInfo.KEY##Height; \
    break;
    scenario_case(NSCam::SENSOR_SCENARIO_ID_NORMAL_PREVIEW, preview, size);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_NORMAL_CAPTURE, capture, size);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_NORMAL_VIDEO, video, size);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_SLIM_VIDEO1, video1, size);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_SLIM_VIDEO2, video2, size);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM1, SensorCustom1, size);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM2, SensorCustom2, size);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM3, SensorCustom3, size);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM4, SensorCustom4, size);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM5, SensorCustom5, size);
#undef scenario_case
    default:
      MY_LOGE("not support sensor scenario(0x%x)", sensorMode);
      return MFALSE;
  }
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::getSensorFps(MUINT32 const sensorMode, MINT32* fps) const {
  return mpImp->getSensorFps(sensorMode, fps);
}

MBOOL
HwInfoHelper::Implementor::getSensorFps(MUINT32 const sensorMode,
                                        MINT32* fps) const {
  switch (sensorMode) {
#define scenario_case(scenario, KEY, fps_var)         \
  case scenario:                                      \
    *fps_var = mSensorStaticInfo.KEY##FrameRate / 10; \
    break;
    scenario_case(NSCam::SENSOR_SCENARIO_ID_NORMAL_PREVIEW, preview, fps);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_NORMAL_CAPTURE, capture, fps);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_NORMAL_VIDEO, video, fps);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_SLIM_VIDEO1, video1, fps);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_SLIM_VIDEO2, video2, fps);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM1, custom1, fps);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM2, custom2, fps);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM3, custom3, fps);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM4, custom4, fps);
    scenario_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM5, custom5, fps);

#undef scenario_case
    default:
      MY_LOGE("not support sensor scenario(0x%x)", sensorMode);
      return MFALSE;
  }
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::getImgoFmt(MUINT32 const bitDepth,
                         MINT* fmt,
                         MBOOL forceUFO,
                         MBOOL useUnpakFmt) const {
  return mpImp->getImgoFmt(bitDepth, fmt, forceUFO, useUnpakFmt);
}

MBOOL
HwInfoHelper::Implementor::getImgoFmt(MUINT32 const bitDepth,
                                      MINT* fmt,
                                      MBOOL forceUFO,
                                      MBOOL useUnpakFmt) const {
#define case_Format(condition, mappedfmt, fmt_var) \
  case condition:                                  \
    *fmt_var = mappedfmt;                          \
    break;

  if (isYuv()) {
    switch (mSensorStaticInfo.sensorFormatOrder) {
      case_Format(NSCam::SENSOR_FORMAT_ORDER_UYVY, NSCam::eImgFmt_UYVY, fmt);
      case_Format(NSCam::SENSOR_FORMAT_ORDER_VYUY, NSCam::eImgFmt_VYUY, fmt);
      case_Format(NSCam::SENSOR_FORMAT_ORDER_YUYV, NSCam::eImgFmt_YUY2, fmt);
      case_Format(NSCam::SENSOR_FORMAT_ORDER_YVYU, NSCam::eImgFmt_YVYU, fmt);
      default:
        MY_LOGE("formatOrder not supported: 0x%x",
                mSensorStaticInfo.sensorFormatOrder);
        return MFALSE;
    }
  } else if (isRaw()) {
    {
      // for debug flow
      MINT32 unpak = ::property_get_int32("debug.camera.rawunpak", -1);
      if (unpak > 0) {
        MY_LOGI(
            "debug.camera.rawunpak = %d, refer useUnpakFmt = %d, forced to use "
            "unpak format",
            unpak, useUnpakFmt);
        useUnpakFmt = MTRUE;
      } else if (unpak == 0) {
        MY_LOGI(
            "debug.camera.rawunpak = %d,  refer useUnpakFmt = %d, don't use "
            "unpak format",
            unpak, useUnpakFmt);
        useUnpakFmt = MFALSE;
      } else {
        MY_LOGI("debug.camera.rawunpak = %d,  useUnpakFmt = %d", unpak,
                useUnpakFmt);
      }
    }
    //
    if (useUnpakFmt) {
      switch (bitDepth) {
        case_Format(8, NSCam::eImgFmt_BAYER8_UNPAK, fmt);
        case_Format(10, NSCam::eImgFmt_BAYER10_UNPAK, fmt);
        case_Format(12, NSCam::eImgFmt_BAYER12_UNPAK, fmt);
        case_Format(14, NSCam::eImgFmt_BAYER14_UNPAK, fmt);
        default:
          MY_LOGE("bitdepth not supported: %d", bitDepth);
          return MFALSE;
      }
    } else {
      MINT imgFmt[2];
      auto pModule = IV4L2PipeFactory::get();
      if (!pModule) {
        MY_LOGE("INormalPipeModule::get() fail");
        return MFALSE;
      }
      NormalPipe_QueryInfo queryRst;
      NormalPipe_QueryIn input;
      pModule->query(PORT_IMGO.index, ENPipeQueryCmd_QUERY_FMT,
                     NSCam::eImgFmt_UNKNOWN, input, &queryRst);

      if (forceUFO) {
        imgFmt[0] = (!mUseUFO || !mUseUFO_IMGO)
                        ? NSCam::eImgFmt_UNKNOWN
                        : bitDepth == 8
                              ? NSCam::eImgFmt_UFO_BAYER8
                              : bitDepth == 10
                                    ? NSCam::eImgFmt_UFO_BAYER10
                                    : bitDepth == 12
                                          ? NSCam::eImgFmt_UFO_BAYER12
                                          : bitDepth == 14
                                                ? NSCam::eImgFmt_UFO_BAYER14
                                                : NSCam::eImgFmt_UNKNOWN;
        imgFmt[1] = bitDepth == 8
                        ? NSCam::eImgFmt_BAYER8
                        : bitDepth == 10
                              ? NSCam::eImgFmt_BAYER10
                              : bitDepth == 12
                                    ? NSCam::eImgFmt_BAYER12
                                    : bitDepth == 14 ? NSCam::eImgFmt_BAYER14
                                                     : NSCam::eImgFmt_UNKNOWN;
      } else {
        imgFmt[0] = NSCam::eImgFmt_UNKNOWN;
        imgFmt[1] = bitDepth == 8
                        ? NSCam::eImgFmt_BAYER8
                        : bitDepth == 10
                              ? NSCam::eImgFmt_BAYER10
                              : bitDepth == 12
                                    ? NSCam::eImgFmt_BAYER12
                                    : bitDepth == 14 ? NSCam::eImgFmt_BAYER14
                                                     : NSCam::eImgFmt_UNKNOWN;
      }

      if (queryRst.query_fmt.size() > 0) {
        std::vector<NSCam::EImageFormat>::iterator it;
        *fmt = NSCam::eImgFmt_UNKNOWN;
        for (int i = 0; i < 2; i++) {
          if (imgFmt[i] == NSCam::eImgFmt_UNKNOWN) {
            continue;
          }

          for (it = queryRst.query_fmt.begin(); it != queryRst.query_fmt.end();
               ++it) {
            if (imgFmt[i] == *it) {
              break;
            }
          }
          if (it != queryRst.query_fmt.end()) {
            *fmt = imgFmt[i];
            break;
          }
        }
      } else {
        *fmt = imgFmt[0] != NSCam::eImgFmt_UNKNOWN ? imgFmt[0] : imgFmt[1];
      }

      if (*fmt == NSCam::eImgFmt_UNKNOWN) {
        MY_LOGE("bitdepth not supported: %d", bitDepth);
        return MFALSE;
      }
    }
  } else {
    MY_LOGE("sensorType not supported yet(0x%x)", mSensorStaticInfo.sensorType);
    return MFALSE;
  }
#undef case_Format
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::getRrzoFmt(MUINT32 const bitDepth,
                         MINT* fmt,
                         MBOOL forceUFO) const {
  return mpImp->getRrzoFmt(bitDepth, fmt, forceUFO);
}

MBOOL
HwInfoHelper::Implementor::getRrzoFmt(MUINT32 const bitDepth,
                                      MINT* fmt,
                                      MBOOL forceUFO) const {
  if (isYuv()) {
    return MFALSE;
  } else if (isRaw()) {
    MINT imgFmt[2];
    auto pModule = IV4L2PipeFactory::get();
    if (!pModule) {
      MY_LOGE("INormalPipeModule::get() fail");
      return MFALSE;
    }
    NormalPipe_QueryInfo queryRst;
    NormalPipe_QueryIn input;
    pModule->query(PORT_RRZO.index, ENPipeQueryCmd_QUERY_FMT,
                   NSCam::eImgFmt_UNKNOWN, input, &queryRst);

    if (forceUFO) {
      imgFmt[0] = (!mUseUFO || !mUseUFO_RRZO)
                      ? NSCam::eImgFmt_UNKNOWN
                      : bitDepth == 8
                            ? NSCam::eImgFmt_UFO_FG_BAYER8
                            : bitDepth == 10
                                  ? NSCam::eImgFmt_UFO_FG_BAYER10
                                  : bitDepth == 12
                                        ? NSCam::eImgFmt_UFO_FG_BAYER12
                                        : bitDepth == 14
                                              ? NSCam::eImgFmt_UFO_FG_BAYER14
                                              : NSCam::eImgFmt_UNKNOWN;
      imgFmt[1] = bitDepth == 8
                      ? NSCam::eImgFmt_FG_BAYER8
                      : bitDepth == 10
                            ? NSCam::eImgFmt_FG_BAYER10
                            : bitDepth == 12
                                  ? NSCam::eImgFmt_FG_BAYER12
                                  : bitDepth == 14 ? NSCam::eImgFmt_FG_BAYER14
                                                   : NSCam::eImgFmt_UNKNOWN;
    } else {
      imgFmt[0] = NSCam::eImgFmt_UNKNOWN;
      imgFmt[1] = bitDepth == 8
                      ? NSCam::eImgFmt_FG_BAYER8
                      : bitDepth == 10
                            ? NSCam::eImgFmt_FG_BAYER10
                            : bitDepth == 12
                                  ? NSCam::eImgFmt_FG_BAYER12
                                  : bitDepth == 14 ? NSCam::eImgFmt_FG_BAYER14
                                                   : NSCam::eImgFmt_UNKNOWN;
    }

    if (queryRst.query_fmt.size() > 0) {
      std::vector<NSCam::EImageFormat>::iterator it;
      *fmt = NSCam::eImgFmt_UNKNOWN;
      for (int i = 0; i < 2; i++) {
        if (imgFmt[i] == NSCam::eImgFmt_UNKNOWN) {
          continue;
        }

        for (it = queryRst.query_fmt.begin(); it != queryRst.query_fmt.end();
             ++it) {
          if (imgFmt[i] == *it) {
            break;
          }
        }
        if (it != queryRst.query_fmt.end()) {
          *fmt = imgFmt[i];
          break;
        }
      }
    } else {
      *fmt = imgFmt[0] != NSCam::eImgFmt_UNKNOWN ? imgFmt[0] : imgFmt[1];
    }

    if (*fmt == NSCam::eImgFmt_UNKNOWN) {
      MY_LOGE("bitdepth not supported: %d", bitDepth);
      return MFALSE;
    }

  } else {
    MY_LOGE("sensorType not supported yet(0x%x)", mSensorStaticInfo.sensorType);
    return MFALSE;
  }
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::getLpModeSupportBitDepthFormat(const MINT& fmt,
                                             MUINT32* pipeBit) const {
  return mpImp->getLpModeSupportBitDepthFormat(fmt, pipeBit);
}

MBOOL
HwInfoHelper::Implementor::getLpModeSupportBitDepthFormat(
    const MINT& fmt, MUINT32* pipeBit) const {
  int pipebitdepth = property_get_int32("debug.camera.pipebitdepth", -1);
  if (pipebitdepth >= 0) {
    *pipeBit = (MUINT32)pipebitdepth;
    MY_LOGD("(For Debug)Force get LP mode supprot bit depth format (0x%x) !",
            *pipeBit);
    return MTRUE;
  }

  auto pModule = IV4L2PipeFactory::get();
  if (!pModule) {
    MY_LOGE("INormalPipeModule::get() fail");
    return MFALSE;
  }

  NormalPipe_QueryInfo queryRst;
  NormalPipe_QueryIn input;
  pModule->query(NSCam::NSIoPipe::PORT_IMGO.index,
                 ENPipeQueryCmd_PIPELINE_BITDEPTH, fmt, input, &queryRst);
  *pipeBit = queryRst.pipelinebitdepth;
  MY_LOGD("get LP mode supprot bit depth format (0x%x) !", *pipeBit);
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::getRecommendRawBitDepth(MINT32* bitDepth) const {
  return mpImp->getRecommendRawBitDepth(bitDepth);
}

MBOOL
HwInfoHelper::Implementor::getRecommendRawBitDepth(MINT32* bitDepth) const {
  *bitDepth = property_get_int32("debug.camera.raw.bitdepth", -1);
  if (*bitDepth == 10) {
    MY_LOGD("force set raw bit 10 bits");
    return MTRUE;
  } else if (*bitDepth == 12) {
    MY_LOGD("force set raw bit 12 bits");
    return MTRUE;
  }

  if (isYuv()) {
    *bitDepth = 10;
    MY_LOGD("isYuv => recommend raw bit 10 bits");
    return MFALSE;
  } else if (isRaw()) {
    auto pModule = IV4L2PipeFactory::get();
    if (!pModule) {
      MY_LOGE("INormalPipeModule::get() fail");
      return MFALSE;
    }

    MINT32 imgFormat = NSCam::eImgFmt_BAYER12;
    MUINT32 queryLpBitFmt = CAM_Pipeline_12BITS;

    getLpModeSupportBitDepthFormat(imgFormat, &queryLpBitFmt);

    if (queryLpBitFmt & CAM_Pipeline_14BITS) {
      *bitDepth = 12;
      MY_LOGD(
          "pipeline bit depth support 14 bits => recommend raw bit 12 bits");
    } else {
      *bitDepth = 10;
      MY_LOGD("recommend raw bit 10 bits");
    }
  } else {
    MY_LOGE("sensorType not supported yet(0x%x)", mSensorStaticInfo.sensorType);
    return MFALSE;
  }
  return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::getSensorPowerOnPredictionResult(MBOOL* isPowerOnSuccess) const {
  return mpImp->getSensorPowerOnPredictionResult(isPowerOnSuccess);
}

MBOOL
HwInfoHelper::Implementor::getSensorPowerOnPredictionResult(
    MBOOL* isPowerOnSuccess) const {
  auto pModule = IV4L2PipeFactory::get();
  if (!pModule) {
    MY_LOGE("INormalPipeModule::get() fail");
    return MFALSE;
  }
  sCAM_QUERY_ISP_RES QueryIn;
  QueryIn.QueryInput.sensorIdx = mOpenId;
  QueryIn.QueryInput.scenarioId = NSCam::SENSOR_SCENARIO_ID_NORMAL_CAPTURE;
  QueryIn.QueryInput.rrz_out_w = DISPLAY_WIDTH;
  QueryIn.QueryInput.pattern = eCAM_NORMAL;
  QueryIn.QueryInput.bin_off = MFALSE;
  if (!pModule->query(ENPipeQueryCmd_ISP_RES, (MUINTPTR)&QueryIn)) {
    MY_LOGE("ISP Query is not supported");
    *isPowerOnSuccess = MTRUE;
    return MFALSE;
  }
  MY_LOGD("SensorId: %d SensorOnPredictionResult: %d", mOpenId,
          QueryIn.QueryOutput);
  *isPowerOnSuccess = QueryIn.QueryOutput;
  return MTRUE;
}
/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::queryPixelMode(MUINT32 const sensorMode,
                             MINT32 const fps,
                             MUINT32* pixelMode) const {
  return mpImp->queryPixelMode(sensorMode, fps, pixelMode);
}

MBOOL
HwInfoHelper::Implementor::queryPixelMode(MUINT32 const sensorMode,
                                          MINT32 const fps,
                                          MUINT32* pixelMode) const {
  IHalSensor* pSensorHalObj = NULL;
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  //
  if (!pHalSensorList) {
    MY_LOGE("pHalSensorList == NULL");
    return MFALSE;
  }

  pSensorHalObj = pHalSensorList->createSensor(LOG_TAG, mOpenId);
  if (pSensorHalObj == NULL) {
    MY_LOGE("pSensorHalObj is NULL");
    return MFALSE;
  }

  pSensorHalObj->sendCommand(pHalSensorList->querySensorDevIdx(mOpenId),
                             NSCam::SENSOR_CMD_GET_SENSOR_PIXELMODE,
                             (MUINTPTR)(&sensorMode), sizeof(MUINT32),
                             (MUINTPTR)(&fps), sizeof(MINT32),
                             (MUINTPTR)(pixelMode), sizeof(MUINT32));

  pSensorHalObj->destroyInstance(LOG_TAG);

  if (*pixelMode != 0 && *pixelMode != 1 && *pixelMode != 2) {
    MY_LOGE("Un-supported pixel mode %d", *pixelMode);
    return MFALSE;
  }

  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::alignPass1HwLimitation(MUINT32 const pixelMode,
                                     MINT const imgFormat,
                                     MBOOL isImgo,
                                     MSize* size,
                                     size_t* stride) const {
  return mpImp->alignPass1HwLimitation(pixelMode, imgFormat, isImgo, size,
                                       stride);
}

MBOOL
HwInfoHelper::Implementor::alignPass1HwLimitation(MUINT32 const pixelMode,
                                                  MINT const imgFormat,
                                                  MBOOL isImgo,
                                                  MSize* size,
                                                  size_t* stride) const {
  auto pModule = IV4L2PipeFactory::get();
  if (!pModule) {
    MY_LOGE("INormalPipeModule::get() fail");
    return MFALSE;
  }

  NormalPipe_QueryInfo queryRst;
  NormalPipe_QueryIn input;
  input.width = size->w;
  input.pix_mode = (pixelMode == 0) ? NSCam::NSIoPipe::NSCamIOPipe::_1_PIX_MODE
                                    : NSCam::NSIoPipe::NSCamIOPipe::_2_PIX_MODE;

  MY_LOGD("format is %x, size=%d", imgFormat, input.width);

  input.img_fmt = (NSCam::EImageFormat)imgFormat;
  pModule->query(isImgo ? PORT_IMGO.index : PORT_RRZO.index,
                 NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_PIX |
                     NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_PIX |
                     NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE,
                 imgFormat, input, &queryRst);

  size->w = queryRst.x_pix;
  size->h = ((size->h + 1) & (~1));
  *stride = queryRst.stride_B[0];
  MY_LOGD("rrzo size %dx%d, stride %zu", size->w, size->h, *stride);
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::alignRrzoHwLimitation(MSize const targetSize,
                                    MSize const sensorSize,
                                    MSize* result) const {
  return mpImp->alignRrzoHwLimitation(targetSize, sensorSize, result);
}

MBOOL
HwInfoHelper::Implementor::alignRrzoHwLimitation(MSize const targetSize,
                                                 MSize const sensorSize,
                                                 MSize* result) const {
  MUINT32 SupportRatio = 6;
  bool scaledUp = false;
  *result = targetSize;

  // figure out the crop region size
  MSize usedRegionSize;
  usedRegionSize.w = sensorSize.w;
  usedRegionSize.h = sensorSize.h;
  // check if the edges are beyond hardware scale limitation(crop region edge *
  // scale ratio) scale up to cope with the limitation, if needed
  // querySupportResizeRatio(SupportRatio);

  // check the width
  if (result->w < ROUND_UP(usedRegionSize.w * SupportRatio, 100)) {
    *result = MSize(
        ROUND_UP(usedRegionSize.w * SupportRatio, 100),
        result->h * ROUND_UP(usedRegionSize.w * SupportRatio, 100) / result->w);
    scaledUp = true;
    ALIGN16(result->w);
    ALIGN16(result->h);
    MY_LOGD(
        "width is beyond scale limitation, modified size: %dx%d, original "
        "target size: %dx%d, crop size: %dx%d",
        result->w, result->h, targetSize.w, targetSize.h, usedRegionSize.w,
        usedRegionSize.h);
  }

  // check the height
  if (result->h < ROUND_UP(usedRegionSize.h * SupportRatio, 100)) {
    *result = MSize(
        result->w * ROUND_UP(usedRegionSize.h * SupportRatio, 100) / result->h,
        ROUND_UP(usedRegionSize.h * SupportRatio, 100));
    scaledUp = true;
    ALIGN16(result->w);
    ALIGN16(result->h);
    MY_LOGD(
        "height is beyond scale limitation, modified size: %dx%d, original "
        "target size: %dx%d, crop size: %dx%d",
        result->w, result->h, targetSize.w, targetSize.h, usedRegionSize.w,
        usedRegionSize.h);
  }

  if (!scaledUp) {
    // we don't attempt to scale down if scaledUp is true,
    // since it means at least one edge is at the limit
    MSize temp = *result;
    if (temp.w > sensorSize.w) {
      temp = MSize(sensorSize.w, temp.h * sensorSize.w / temp.w);
    }

    if (temp.h > sensorSize.h) {
      temp = MSize(temp.w * sensorSize.h / temp.h, sensorSize.h);
    }

    if (temp.w > usedRegionSize.w * SupportRatio / 100 &&
        temp.h > usedRegionSize.h * SupportRatio / 100) {
      *result = temp;
      MY_LOGD(
          "exceeding sensor size, modified size: %dx%d, original target size: "
          "%dx%d, crop size: %dx%d",
          result->w, result->h, targetSize.w, targetSize.h, usedRegionSize.w,
          usedRegionSize.h);
    }
  }
#undef ROUND_UP
#undef ALIGN16
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::quertMaxRrzoWidth(MINT32* maxWidth) const {
  return mpImp->quertMaxRrzoWidth(maxWidth);
}

MBOOL
HwInfoHelper::Implementor::quertMaxRrzoWidth(MINT32* maxWidth) const {
#define MAX_RRZO_W (3264)

  *maxWidth = MAX_RRZO_W;
  MUINT32 ret = true;

  auto pModule = IV4L2PipeFactory::get();
  if (!pModule) {
    MY_LOGE("INormalPipeModule::get() fail");
    return MFALSE;
  }

  NSCam::NSIoPipe::NSCamIOPipe::sCAM_QUERY_MAX_PREVIEW_SIZE MaxWidth;
  MaxWidth.QueryOutput = MAX_RRZO_W;
  ret = pModule->query(
      NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_MAX_PREVIEW_SIZE,
      (MUINTPTR)&MaxWidth);

  if (!ret) {
    MY_LOGW(
        "this platform not support ENPipeQueryCmd_MAX_PREVIEW_SIZE, use "
        "default value : %d",
        MAX_RRZO_W);
    *maxWidth = MAX_RRZO_W;
    return MFALSE;
  }
  *maxWidth = MaxWidth.QueryOutput;

  return MTRUE;
#undef MAX_RRZO_W
}

/*****************************************************************************
 *
 ****************************************************************************/
MBOOL
HwInfoHelper::querySupportVHDRMode(MUINT32 const sensorMode,
                                   MUINT32* vhdrMode) const {
  return mpImp->querySupportVHDRMode(sensorMode, vhdrMode);
}

MBOOL
HwInfoHelper::Implementor::querySupportVHDRMode(MUINT32 const sensorMode,
                                                MUINT32* vhdrMode) const {
  IHalSensor* pSensorHalObj = NULL;
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  //
  if (!pHalSensorList) {
    MY_LOGE("pHalSensorList == NULL");
    return MFALSE;
  }

  pSensorHalObj = pHalSensorList->createSensor(LOG_TAG, mOpenId);
  if (pSensorHalObj == NULL) {
    MY_LOGE("pSensorHalObj is NULL");
    return MFALSE;
  }

  pSensorHalObj->sendCommand(pHalSensorList->querySensorDevIdx(mOpenId),
                             NSCam::SENSOR_CMD_GET_SENSOR_HDR_CAPACITY,
                             (MUINTPTR)(&sensorMode), sizeof(MUINT32),
                             (MUINTPTR)(vhdrMode), sizeof(MUINT32), (MUINTPTR)0,
                             sizeof(MUINT32));

  return MTRUE;
}

/******************************************************************************
 *
 *
 *****************************************************************************/
MBOOL
HwInfoHelper::queryUFOStride(MINT const imgFormat,
                             MSize const imgSize,
                             size_t stride[3]) const {
  return mpImp->queryUFOStride(imgFormat, imgSize, stride);
}

MBOOL
HwInfoHelper::Implementor::queryUFOStride(MINT const imgFormat,
                                          MSize const imgSize,
                                          size_t stride[3]) const {
  auto pModule = IV4L2PipeFactory::get();
  if (!pModule) {
    MY_LOGE("INormalPipeModule::get() fail");
    return MFALSE;
  }
  auto portIndex = 0;
  switch (imgFormat) {
    case NSCam::eImgFmt_UFO_BAYER8:
    case NSCam::eImgFmt_UFO_BAYER10:
    case NSCam::eImgFmt_UFO_BAYER12:
    case NSCam::eImgFmt_UFO_BAYER14:
      portIndex = PORT_IMGO.index;
      break;
    case NSCam::eImgFmt_UFO_FG_BAYER8:
    case NSCam::eImgFmt_UFO_FG_BAYER10:
    case NSCam::eImgFmt_UFO_FG_BAYER12:
    case NSCam::eImgFmt_UFO_FG_BAYER14:
      portIndex = PORT_RRZO.index;
      break;
    default:
      MY_LOGE("Not UFO format!");
      return MFALSE;
  }
  NormalPipe_QueryInfo queryRst;
  NormalPipe_QueryIn input;
  input.width = imgSize.w;  // pixMode as default
  pModule->query(portIndex, ENPipeQueryCmd_STRIDE_BYTE, imgFormat, input,
                 &queryRst);
  for (int i = 0; i < 3; i++) {
    stride[i] = (size_t)queryRst.stride_B[i];
  }
  return MTRUE;
}

/****************************************************************************
 *
 *
 ***************************************************************************/
MBOOL
HwInfoHelper::getSensorRawFmtType(MUINT32* u4RawFmtType) const {
  return mpImp->getSensorRawFmtType(u4RawFmtType);
}

MBOOL
HwInfoHelper::Implementor::getSensorRawFmtType(MUINT32* u4RawFmtType) const {
  SensorStaticInfo sensorStaticInfo;
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  if (!pHalSensorList) {
    MY_LOGE("pHalSensorList == NULL");
    return MFALSE;
  }
  //
  MUINT32 sensorDev = pHalSensorList->querySensorDevIdx(mOpenId);
  pHalSensorList->querySensorStaticInfo(sensorDev, &sensorStaticInfo);
  *u4RawFmtType = sensorStaticInfo.rawFmtType;
  MY_LOGD("SensorStaticInfo SensorRawFmtType(%d)", *u4RawFmtType);

  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::getShutterDelayFrameCount(MINT32* shutterDelayCnt) const {
  return mpImp->getShutterDelayFrameCount(shutterDelayCnt);
}
MBOOL
HwInfoHelper::Implementor::getShutterDelayFrameCount(
    MINT32* shutterDelayCnt) const {
  *shutterDelayCnt = 0;
  SensorStaticInfo sensorStaticInfo;
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  if (!pHalSensorList) {
    MY_LOGE("pHalSensorList == NULL");
    return MFALSE;
  }
  //
  MUINT32 sensorDev = pHalSensorList->querySensorDevIdx(mOpenId);
  pHalSensorList->querySensorStaticInfo(sensorDev, &sensorStaticInfo);
  int i4AeShutDelayFrame = sensorStaticInfo.aeShutDelayFrame;
  int i4AeISPGainDelayFrame = sensorStaticInfo.aeISPGainDelayFrame;
  *shutterDelayCnt = (i4AeISPGainDelayFrame - i4AeShutDelayFrame);
  //
  MY_LOGD(
      "i4AeISPGainDelayFrame(%d) i4AeShutDelayFrame(%d) shutterDelayCnt(%d)",
      i4AeISPGainDelayFrame, i4AeShutDelayFrame, *shutterDelayCnt);
  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::shrinkCropRegion(NSCam::MSize const sensorSize,
                               NSCam::MRect* cropRegion,
                               MINT32 shrinkPx) const {
  return mpImp->shrinkCropRegion(sensorSize, cropRegion, shrinkPx);
}
MBOOL
HwInfoHelper::Implementor::shrinkCropRegion(NSCam::MSize const sensorSize,
                                            NSCam::MRect* cropRegion,
                                            MINT32 shrinkPx) const {
  if ((sensorSize.w - shrinkPx) <= cropRegion->width()) {
    cropRegion->p.x = shrinkPx;
    cropRegion->s.w = (sensorSize.w - shrinkPx * 2);
  }
  if ((sensorSize.h - shrinkPx) <= cropRegion->height()) {
    cropRegion->p.y = shrinkPx;
    cropRegion->s.h = (sensorSize.h - shrinkPx * 2);
  }
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::querySupportResizeRatio(MUINT32* rPrecentage) const {
  MBOOL ret = mpImp->querySupportResizeRatio(rPrecentage);
  return ret;
}

MBOOL
HwInfoHelper::Implementor::querySupportResizeRatio(MUINT32* rPrecentage) const {
  auto pModule = IV4L2PipeFactory::get();
  *rPrecentage = 40;
  if (!pModule) {
    MY_LOGE("INormalPipeModule::get() fail, default value = 40");
    return MFALSE;
  }

  MBOOL ret = MFALSE;
  NSCam::NSIoPipe::NSCamIOPipe::sCAM_QUERY_BS_RATIO info;
  info.QueryInput.portId = NSImageio::NSIspio::EPortIndex_RRZO;
  ret = pModule->query(NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_BS_RATIO,
                       (MUINTPTR)&info);
  if (!ret) {
    MY_LOGW(
        "Cannot query ENPipeQueryCmd_BS_RATIO from DRV, default value = 40");
  } else {
    *rPrecentage = info.QueryOutput;
    MY_LOGD("Support Resize-Ratio-Percentage: %d", *rPrecentage);
  }
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::querySupportBurstNum(MUINT32* rBitField) const {
  MBOOL ret = mpImp->querySupportBurstNum(rBitField);
  return ret;
}

MBOOL
HwInfoHelper::Implementor::querySupportBurstNum(MUINT32* rBitField) const {
  auto pModule = IV4L2PipeFactory::get();
  if (!pModule) {
    MY_LOGE("INormalPipeModule::get() fail");
    return MFALSE;
  }

  MBOOL ret = MFALSE;
  sCAM_QUERY_BURST_NUM res;
  res.QueryOutput = 0x0;
  ret = pModule->query(NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_BURST_NUM,
                       (MUINTPTR)(&res));
  if (!ret) {
    MY_LOGW("Cannot query ENPipeQueryCmd_BURST_NUM from DRV");
  } else {
    *rBitField = res.QueryOutput;
    *rBitField |= 0x1;  // always support BurstNum=1
    MY_LOGD("Support Burst-Num-Set: 0x%X (0x%X)", *rBitField, res.QueryOutput);
  }
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::querySupportRawPattern(MUINT32* rBitField) const {
  MBOOL ret = mpImp->querySupportRawPattern(rBitField);
  return ret;
}

MBOOL
HwInfoHelper::Implementor::querySupportRawPattern(MUINT32* rBitField) const {
  auto pModule = IV4L2PipeFactory::get();
  if (!pModule) {
    MY_LOGE("INormalPipeModule::get() fail");
    return MFALSE;
  }

  MBOOL ret = MFALSE;
  sCAM_QUERY_SUPPORT_PATTERN res;
  res.QueryOutput = 0x0;
  ret = pModule->query(
      NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_SUPPORT_PATTERN,
      (MUINTPTR)(&res));
  if (!ret) {
    MY_LOGW("Cannot query ENPipeQueryCmd_SUPPORT_PATTERN from DRV");
  } else {
    *rBitField = res.QueryOutput;
    MY_LOGD("Support Raw-Pattern-Set: 0x%X", *rBitField);
  }
  return ret;
}

/******************************************************************************
 *
 *
 ******************************************************************************/
MBOOL
HwInfoHelper::getDynamicTwinSupported() {
  static MBOOL ret = HwInfoHelper::Implementor::getDynamicTwinSupported();
  return ret;
}
MBOOL
HwInfoHelper::Implementor::getDynamicTwinSupported() {
  auto pModule = IV4L2PipeFactory::get();
  if (!pModule) {
    MY_LOGE("INormalPipeModule::get() fail");
    return MFALSE;
  }

  MBOOL ret = MFALSE;
  NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo info;
  pModule->query(0, NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_D_Twin, 0, 0,
                 &info);
  ret = info.D_TWIN;

  MY_LOGD("[%s] is support dynamic twin: %s", __FUNCTION__,
          ret ? "true" : "false");
  return ret;
}

/******************************************************************************
 *
 *
 ******************************************************************************/

MUINT32
HwInfoHelper::getCameraSensorPowerOnCount() {
  return HwInfoHelper::Implementor::getCameraSensorPowerOnCount();
}
MUINT32
HwInfoHelper::Implementor::getCameraSensorPowerOnCount() {
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  if (!pHalSensorList) {
    MY_LOGE("pHalSensorList == NULL");
    return MFALSE;
  }
  // get total physic camera sensor.
  MUINT sensorCount = pHalSensorList->queryNumberOfSensors();
  IHalSensor* pSensorHalObj = nullptr;
  MUINT32 powerOnResult = 0;
  MUINT32 powerOnCount = 0;
  for (MUINT32 i = 0; i < sensorCount; ++i) {
    pSensorHalObj = pHalSensorList->createSensor(LOG_TAG, i);
    if (pSensorHalObj == NULL) {
      MY_LOGE("pSensorHalObj is NULL");
      return MFALSE;
    }

    pSensorHalObj->sendCommand(pHalSensorList->querySensorDevIdx(i),
                               NSCam::SENSOR_CMD_GET_SENSOR_POWER_ON_STATE,
                               (MUINTPTR)(&powerOnResult), sizeof(MUINT32), 0,
                               sizeof(MUINT32), 0, sizeof(MUINT32));
    if (powerOnResult > 0) {
      powerOnCount++;
    }
    pSensorHalObj->destroyInstance(LOG_TAG);
  }
  MY_LOGD("powerOnCount(%d) sensorCount(%d)", powerOnCount, sensorCount);
  return powerOnCount;
}

/******************************************************************************
 *
 ******************************************************************************/
HwInfoHelper::HwInfoHelper(MINT32 const openId) {
  mpImp = new Implementor(openId);
}

/******************************************************************************
 *
 ******************************************************************************/
HwInfoHelper::~HwInfoHelper() {
  delete mpImp;
}
