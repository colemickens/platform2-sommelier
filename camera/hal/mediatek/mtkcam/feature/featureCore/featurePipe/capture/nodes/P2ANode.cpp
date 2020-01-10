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

#include <common/include/DebugControl.h>

#define PIPE_CLASS_TAG "P2ANode"
#define PIPE_TRACE TRACE_P2A_NODE
#include <algorithm>
#include <cmath>
#include <common/include/PipeLog.h>
#include <isp_tuning/isp_tuning.h>
#include <INormalStream.h>

#include <memory>
#include <mtkcam/feature/featureCore/featurePipe/capture/nodes/P2ANode.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/drv/def/Dip_Notify_datatype.h>

#include <mtkcam/pipeline/stream/StreamId.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>

#include <mtkcam/utils/TuningUtils/FileDumpNamingRule.h>
#include <property_lib.h>
#include <string>
#include <utility>
#include "../thread/CaptureTaskQueue.h"

using NSCam::TuningUtils::FILE_DUMP_NAMING_HINT;
using NSCam::TuningUtils::RAW_PORT;
using NSCam::TuningUtils::RAW_PORT_IMGO;
using NSCam::TuningUtils::RAW_PORT_NULL;
using NSCam::TuningUtils::RAW_PORT_RRZO;
using NSCam::TuningUtils::YUV_PORT;
using NSCam::TuningUtils::YUV_PORT_IMG2O;
using NSCam::TuningUtils::YUV_PORT_IMG3O;
using NSCam::TuningUtils::YUV_PORT_NULL;
using NSCam::TuningUtils::YUV_PORT_WDMAO;
using NSCam::TuningUtils::YUV_PORT_WROTO;
using NSCam::v4l2::ENormalStreamTag_Vss;
using NSIspTuning::EIspProfile_Capture_MultiPass_HWNR;
using NSIspTuning::EIspProfile_YUV_Reprocess;

using NSCam::NSIoPipe::PORT_DEPI;
using NSCam::NSIoPipe::PORT_IMG2O;
using NSCam::NSIoPipe::PORT_IMG3O;
using NSCam::NSIoPipe::PORT_IMGBI;
using NSCam::NSIoPipe::PORT_IMGCI;
using NSCam::NSIoPipe::PORT_IMGI;
using NSCam::NSIoPipe::PORT_LCEI;
using NSCam::NSIoPipe::PORT_TUNING;
using NSCam::NSIoPipe::PORT_WDMAO;
using NSCam::NSIoPipe::PORT_WROTO;

using NSCam::v3::IImageStreamInfo;
using NSCam::v3::Utils::ImageStreamInfo;

#define ISP30_TOLERANCE_CROP_OFFSET (128)
#define ISP30_TOLERANCE_RESIZE_RATIO (8)
#define P2LIMITED (1)
#define CAPTURE_CACHE_BUFFER_NUM 6

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

enum DeubgMode {
  AUTO = -1,
  OFF = 0,
  ON = 1,
};

P2ANode::P2ANode(NodeID_T nid, const char* name)
    : CaptureFeatureNode(nid, name),
      mp3AHal(nullptr),
      mp3AHal2(nullptr),
      mpP2Opt(nullptr),
      mpP2Opt2(nullptr)
#if MTK_DP_ENABLE
      ,
      mDipVer(0),
      mISP3_0(MFALSE)
#endif
      ,
      mHasAllocDip(MFALSE),
      mTaskQueue(nullptr) {
  TRACE_FUNC_ENTER();
  this->addWaitQueue(&mRequests);

  mDebugDS = property_get_int32("vendor.debug.camera.ds.enable", -1);
  mDebugDSRatio = property_get_int32("vendor.debug.camera.ds.ratio", 2);

  mDebugDump = property_get_int32("vendor.debug.camera.p2.dump", 0) > 0;

  mForceImg3o = property_get_int32("vendor.debug.camera.p2.force.img3o", 0) > 0;

  mForceImg3o422 =
      property_get_int32("vendor.debug.camera.p2.force.img3o.format422", 0) > 0;

  mDebugImg3o = property_get_int32("vendor.debug.camera.img3o.dump", 0) > 0;

  TRACE_FUNC_EXIT();
}

P2ANode::~P2ANode() {
  TRACE_FUNC_ENTER();

  TRACE_FUNC_EXIT();
}

MVOID P2ANode::setBufferPool(const std::shared_ptr<CaptureBufferPool>& pool) {
  TRACE_FUNC_ENTER();
  mpBufferPool = pool;
  TRACE_FUNC_EXIT();
}

MBOOL P2ANode::configNormalStream(const StreamConfigure config) {
  MBOOL ret = MFALSE;

  StreamConfigure normal;
  StreamConfigure reprocessing;

  std::shared_ptr<ImageStreamInfo> fullYuv;
  std::shared_ptr<ImageStreamInfo> reprocessYuv;

  for (auto& it : config.mInStreams) {
    if (it != nullptr) {
      switch (it->getImgFormat()) {
        case NSCam::eImgFmt_YV12:
        case NSCam::eImgFmt_NV12:
        case NSCam::eImgFmt_NV21:
        case NSCam::eImgFmt_YUY2:
        case NSCam::eImgFmt_Y8:
        case NSCam::eImgFmt_Y16:
          reprocessing.mInStreams.push_back(it);
          break;
        case NSCam::eImgFmt_CAMERA_OPAQUE:
        case NSCam::eImgFmt_BAYER8:
        case NSCam::eImgFmt_BAYER10:
        case NSCam::eImgFmt_BAYER12:
        case NSCam::eImgFmt_BAYER14:
        case NSCam::eImgFmt_FG_BAYER8:
        case NSCam::eImgFmt_FG_BAYER10:
        case NSCam::eImgFmt_FG_BAYER12:
        case NSCam::eImgFmt_FG_BAYER14:
          normal.mInStreams.push_back(it);
          fullYuv = std::make_shared<ImageStreamInfo>(
              "Hal:Image:Main-YUV", eSTREAMID_BEGIN_OF_INTERNAL,
              v3::eSTREAMTYPE_IMAGE_OUT, 8, 2, 0, eImgFmt_YV12,
              it->getImgSize(), it->getBufPlanes());
          normal.mOutStreams.push_back(fullYuv);
          break;
        default:
          MY_LOGE("Unsupported format:0x%x", it->getImgFormat());
      }
    }
  }
  if (reprocessing.mInStreams.size() > 0) {
    reprocessYuv = std::make_shared<ImageStreamInfo>(
        "Hal:Image:REPROCESS-YUV", eSTREAMID_BEGIN_OF_INTERNAL,
        v3::eSTREAMTYPE_IMAGE_OUT, 8, 2, 0, eImgFmt_YV12,
        reprocessing.mInStreams[0]->getImgSize(),
        reprocessing.mInStreams[0]->getBufPlanes());
    reprocessing.mOutStreams.push_back(reprocessYuv);
  }

  for (auto& it : normal.mInStreams) {
    MY_LOGI("RAW->YUV input %dx%d", it->getImgSize().w, it->getImgSize().h);
  }
  for (auto& it : normal.mOutStreams) {
    MY_LOGI("RAW->YUV output %dx%d", it->getImgSize().w, it->getImgSize().h);
  }
  for (auto& it : reprocessing.mInStreams) {
    MY_LOGI("YUV->YUV input %dx%d", it->getImgSize().w, it->getImgSize().h);
  }
  for (auto& it : reprocessing.mOutStreams) {
    MY_LOGI("YUV->YUV output %dx%d", it->getImgSize().w, it->getImgSize().h);
  }

  if (normal.mInStreams.size() > 0 && normal.mOutStreams.size() > 0) {
    ret = mpP2Opt->configNormalStream(v4l2::ENormalStreamTag_Cap, normal);
  }

  if (reprocessing.mInStreams.size() > 0 &&
      reprocessing.mOutStreams.size() > 0) {
    ret = mpP2ReqOpt->configNormalStream(v4l2::ENormalStreamTag_Rep,
                                         reprocessing);
  }

  return ret;
}

MBOOL P2ANode::onInit() {
  TRACE_FUNC_ENTER();
  CaptureFeatureNode::onInit();

  mTaskQueue = std::make_unique<CaptureTaskQueue>();
  MAKE_Hal3A(
      mp3AHal, [](IHal3A* p) { p->destroyInstance("cfp_3a"); }, mSensorIndex,
      "cfp_3a");
  mpP2Opt = std::make_shared<P2Operator>("normal", mSensorIndex);
  mpP2ReqOpt = std::make_shared<P2Operator>("reprocessing", mSensorIndex);

  mISP3_0 = MFALSE;

  for (auto& it : mDipBuffers) {
    it->unlockBuf("V4L2");
  }

  mHasAllocDip = MFALSE;
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::onUninit() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

/*******************************************************************************
 *
 *****************************************************************************/
MBOOL
P2ANode::enqueISP(const RequestPtr& pRequest,
                  const std::shared_ptr<P2EnqueData>& pEnqueData) {
  TRACE_FUNC_ENTER();

  MERROR ret = OK;
  P2EnqueData& enqueData = *pEnqueData.get();

  // Trigger Dump
  enqueData.mDebugDump = mDebugDump;
  MINT32& frameNo = enqueData.mFrameNo;
  MINT32& requestNo = enqueData.mRequestNo;

  MBOOL master = enqueData.mSensorId == mSensorIndex;

  std::shared_ptr<CaptureFeatureNodeRequest> pNodeReq =
      pRequest->getNodeRequest(NID_P2A);

  auto GetBuffer = [&](IImageBuffer*& rpBuf,
                       BufferID_T bufId) -> IImageBuffer* {
    if (rpBuf) {
      return rpBuf;
    }
    if (bufId != NULL_BUFFER) {
      rpBuf = pNodeReq->acquireBuffer(bufId);
    }
    return rpBuf;
  };

  EnquePackage* pPackage = NULL;

#define CHECK_AND_RELEASE(predict, mesg) \
  do {                                   \
    if (!(predict)) {                    \
      if (pPackage != NULL)              \
        delete pPackage;                 \
      MY_LOGE(mesg);                     \
      return MFALSE;                     \
    }                                    \
  } while (0)

  // 1.input & output data
  IImageBuffer* pIMG2O =
      GetBuffer(enqueData.mIMG2O.mpBuf, enqueData.mIMG2O.mBufId);
  IImageBuffer* pIMG3O =
      GetBuffer(enqueData.mIMG3O.mpBuf, enqueData.mIMG3O.mBufId);
  IImageBuffer* pWROTO =
      GetBuffer(enqueData.mWROTO.mpBuf, enqueData.mWROTO.mBufId);
  IImageBuffer* pWDMAO =
      GetBuffer(enqueData.mWDMAO.mpBuf, enqueData.mWDMAO.mBufId);
  IImageBuffer* pCopy1 =
      GetBuffer(enqueData.mCopy1.mpBuf, enqueData.mCopy1.mBufId);
  IImageBuffer* pCopy2 =
      GetBuffer(enqueData.mCopy2.mpBuf, enqueData.mCopy2.mBufId);

  CHECK_AND_RELEASE(!!pIMG2O || !!pIMG3O || !!pWROTO || !!pWDMAO,
                    "do not acquire a output buffer");

  IMetadata* pIMetaDynamic = enqueData.mpIMetaDynamic;
  IMetadata* pIMetaApp = enqueData.mpIMetaApp;
  IMetadata* pIMetaHal = enqueData.mpIMetaHal;
  IMetadata* pOMetaApp = enqueData.mpOMetaApp;
  IMetadata* pOMetaHal = enqueData.mpOMetaHal;

  IImageBuffer* pIMGI =
      GetBuffer(enqueData.mIMGI.mpBuf, enqueData.mIMGI.mBufId);
  IImageBuffer* pLCEI =
      GetBuffer(enqueData.mLCEI.mpBuf, enqueData.mLCEI.mBufId);

  CHECK_AND_RELEASE(!!pIMGI, "do not acquire a input buffer");

  // 2. Prepare enque package
  std::shared_ptr<IImageBuffer> tunning;
  if (enqueData.mYuvRep) {
    tunning = mpP2ReqOpt->getTuningBuffer();
  } else {
    tunning = mpP2Opt->getTuningBuffer();
  }
  pPackage = new EnquePackage();
  pPackage->mpEnqueData = pEnqueData;
  pPackage->mTuningData = tunning;
  pPackage->mpNode = this;

  // 3. Crop Calculation & add log
  const MSize& rImgiSize = pIMGI->getImgSize();
  std::string strEnqueLog;
  strEnqueLog += base::StringPrintf(
      "Sensor(%d) Resized(%d) Reprocessing(%d) R/F Num: %d/%d, EnQ: IMGI "
      "Fmt(0x%x) Size(%dx%d) VA/PA(%#" PRIxPTR "/%#" PRIxPTR ")",
      enqueData.mSensorId, enqueData.mResized, enqueData.mYuvRep,
      pRequest->getRequestNo(), pRequest->getFrameNo(), pIMGI->getImgFormat(),
      rImgiSize.w, rImgiSize.h, pIMGI->getBufVA(0), pIMGI->getBufPA(0));

  std::shared_ptr<CropCalculator::Factor> pFactor;
  if (enqueData.mWDMAO.mHasCrop || enqueData.mWROTO.mHasCrop ||
      enqueData.mIMG2O.mHasCrop || enqueData.mCopy1.mHasCrop ||
      enqueData.mCopy2.mHasCrop) {
    pFactor = mpCropCalculator->getFactor(pIMetaApp, pIMetaHal);
    CHECK_AND_RELEASE(pFactor != NULL, "can not get crop factor!");

    if (pOMetaApp != NULL) {
      MRect cropRegion = pFactor->mActiveCrop;
      // Update crop region to output app metadata
      trySetMetadata<MRect>(pOMetaApp, MTK_SCALER_CROP_REGION, cropRegion);
    }
  }

  auto GetCropRegion = [&](const char* sPort, P2Output& rOut,
                           IImageBuffer* pImg) mutable {
    if (pImg == NULL) {
      return;
    }

    if (rOut.mHasCrop) {
      MSize cropSize = pImg->getImgSize();
      if (rOut.mTrans & eTransform_ROT_90) {
        swap(cropSize.w, cropSize.h);
      }

      mpCropCalculator->evaluate(pFactor, cropSize, &(rOut.mCropRegion),
                                 enqueData.mResized);
    } else {
      rOut.mCropRegion = MRect(rImgiSize.w, rImgiSize.h);
    }

    strEnqueLog += base::StringPrintf(
        ", %s Rot(%d) Crop(%d,%d)(%dx%d) Size(%dx%d) VA/PA(%#" PRIxPTR
        "/%#" PRIxPTR ")",
        sPort, rOut.mTrans, rOut.mCropRegion.p.x, rOut.mCropRegion.p.y,
        rOut.mCropRegion.s.w, rOut.mCropRegion.s.h, pImg->getImgSize().w,
        pImg->getImgSize().h, pImg->getBufVA(0), pImg->getBufPA(0));
  };

  GetCropRegion("WDMAO", enqueData.mWDMAO, pWDMAO);
  GetCropRegion("WROTO", enqueData.mWROTO, pWROTO);
  GetCropRegion("IMG2O", enqueData.mIMG2O, pIMG2O);
  GetCropRegion("IMG3O", enqueData.mIMG3O, pIMG3O);
  GetCropRegion("COPY1", enqueData.mCopy1, pCopy1);
  GetCropRegion("COPY2", enqueData.mCopy2, pCopy2);

  MY_LOGI("%s", strEnqueLog.c_str());

  // 3.1 ISP tuning
  TuningParam tuningParam = {NULL, NULL};
  {
    // For NDD
    trySetMetadata<MINT32>(pIMetaHal, MTK_PIPELINE_FRAME_NUMBER, frameNo);
    trySetMetadata<MINT32>(pIMetaHal, MTK_PIPELINE_REQUEST_NUMBER, requestNo);

    MUINT8 uIspProfile;

    // For Down Sacle
    if (enqueData.mYuvRep) {
      trySetMetadata<MUINT8>(pIMetaHal, MTK_3A_ISP_PROFILE,
                             EIspProfile_YUV_Reprocess);
    } else if (enqueData.mScaleUp) {
      trySetMetadata<MUINT8>(pIMetaHal, MTK_3A_ISP_PROFILE,
                             EIspProfile_Capture_MultiPass_HWNR);
      MINT32 resolution = enqueData.mScaleUpSize.w | enqueData.mScaleUpSize.h
                                                         << 16;
      trySetMetadata<MINT32>(pIMetaHal, MTK_ISP_P2_IN_IMG_RES_REVISED,
                             resolution);
      // 0 or not exist: RAW->YUV, 1: YUV->YUV
      trySetMetadata<MINT32>(pIMetaHal, MTK_ISP_P2_IN_IMG_FMT, 1);
      MY_LOGD("apply profile(MultiPass_HWNR) revised resolution: 0x%x",
              resolution);
    } else {
      uIspProfile = NSIspTuning::EIspProfile_Capture;
      trySetMetadata<MUINT8>(pIMetaHal, MTK_3A_ISP_PROFILE, uIspProfile);
    }

    // Consturct tuning parameter
    {
      tuningParam.pRegBuf = reinterpret_cast<void*>(tunning->getBufVA(0));
      tuningParam.RegBuf_fd = tunning->getFD();

      // LCEI
      if (pLCEI != NULL) {
        CHECK_AND_RELEASE(!enqueData.mResized,
                          "use LCSO for RRZO buffer, should not happended!");
        tuningParam.pLcsBuf = reinterpret_cast<void*>(pLCEI);
      }

      // USE resize raw-->set PGN 0
      if (enqueData.mResized) {
        trySetMetadata<MUINT8>(pIMetaHal, MTK_3A_PGN_ENABLE, 0);
      } else {
        trySetMetadata<MUINT8>(pIMetaHal, MTK_3A_PGN_ENABLE, 1);
      }
      MetaSet_T inMetaSet;
      inMetaSet.appMeta = *pIMetaApp;
      inMetaSet.halMeta = *pIMetaHal;

      MetaSet_T outMetaSet;
      if (master) {
        ret = mp3AHal->setIsp(0, inMetaSet, &tuningParam, &outMetaSet);
      } else {
        ret = mp3AHal2->setIsp(0, inMetaSet, &tuningParam, &outMetaSet);
      }

      CHECK_AND_RELEASE(ret == OK, "fail to set ISP");

      if (pOMetaHal != NULL) {
        (*pOMetaHal) = inMetaSet.halMeta + outMetaSet.halMeta;
#if MTK_ISP_SUPPORT_COLOR_SPACE
        // If flag on, the NVRAM will always prepare tuning data for P3 color
        // space
        trySetMetadata<MINT32>(pOMetaHal, MTK_ISP_COLOR_SPACE,
                               MTK_ISP_COLOR_SPACE_DISPLAY_P3);
#endif
      }
      if (pOMetaApp != NULL) {
        (*pOMetaApp) += outMetaSet.appMeta;
      }
    }
  }

  // 3.2 Fill PQ Param
  {
    MINT32 iIsoValue = 0;
    if (!tryGetMetadata<MINT32>(pIMetaDynamic, MTK_SENSOR_SENSITIVITY,
                                &iIsoValue)) {
      MY_LOGW("can not get iso value");
    }

    MINT32 iMagicNum = 0;
    if (!tryGetMetadata<MINT32>(pIMetaHal, MTK_P1NODE_PROCESSOR_MAGICNUM,
                                &iMagicNum)) {
      MY_LOGW("can not get magic number");
    }

    MINT32 iRealLv = 0;
    if (!tryGetMetadata<MINT32>(pIMetaHal, MTK_REAL_LV, &iRealLv)) {
      MY_LOGW("can not get read lv");
    }
  }

  // 3.3 Srz tuning for Imgo (LCE not applied to rrzo)
  if (!enqueData.mScaleUp && !enqueData.mResized) {
    auto fillSRZ = [&]() -> ModuleInfo* {
      // srz4 config
      // ModuleInfo srz4_module;
      ModuleInfo* p = new ModuleInfo();
      p->moduleTag = EDipModule_SRZ4;
      p->frameGroup = 0;

      _SRZ_SIZE_INFO_* pSrzParam = new _SRZ_SIZE_INFO_;
      if (pLCEI != NULL) {
        pSrzParam->in_w = pLCEI->getImgSize().w;
        pSrzParam->in_h = pLCEI->getImgSize().h;
        pSrzParam->crop_w = pLCEI->getImgSize().w;
        pSrzParam->crop_h = pLCEI->getImgSize().h;
      }
      pSrzParam->out_w = pIMGI->getImgSize().w;
      pSrzParam->out_h = pIMGI->getImgSize().h;

      p->moduleStruct = reinterpret_cast<MVOID*>(pSrzParam);

      return p;
    };
    pPackage->mpModuleInfo = fillSRZ();
  }

  // 4.create enque param
  NSIoPipe::QParams qParam;

  // 4.1 QParam template
  MINT32 iFrameNum = 0;
  QParamTemplateGenerator qPTempGen = QParamTemplateGenerator(
      iFrameNum, enqueData.mSensorId, ENormalStreamTag_Vss);

  qPTempGen.addInput(PORT_IMGI);
  if (tunning != nullptr)
    qPTempGen.addInput(PORT_TUNING);
  if (!enqueData.mScaleUp && !enqueData.mResized &&
      tuningParam.pLsc2Buf != NULL) {
    qPTempGen.addInput(PORT_IMGCI);
  }

  if (!enqueData.mScaleUp && !enqueData.mResized && pLCEI != NULL) {
    qPTempGen.addInput(PORT_LCEI);
    if (pPackage->mpModuleInfo != NULL) {
      qPTempGen.addModuleInfo(EDipModule_SRZ4,
                              pPackage->mpModuleInfo->moduleStruct);
      qPTempGen.addInput(PORT_DEPI);
    }
  }

  if (!enqueData.mScaleUp && tuningParam.pBpc2Buf != NULL) {
    qPTempGen.addInput(PORT_IMGBI);
  }

  if (pIMG2O != NULL) {
    qPTempGen.addOutput(PORT_IMG2O, 0);
    qPTempGen.addCrop(eCROP_CRZ, enqueData.mIMG2O.mCropRegion.p,
                      enqueData.mIMG2O.mCropRegion.s, pIMG2O->getImgSize());
  }

  if (pIMG3O != NULL) {
    qPTempGen.addOutput(PORT_IMG3O, 0);
  }

  if (pWROTO != NULL) {
    qPTempGen.addOutput(PORT_WROTO, enqueData.mWROTO.mTrans);
    qPTempGen.addCrop(eCROP_WROT, enqueData.mWROTO.mCropRegion.p,
                      enqueData.mWROTO.mCropRegion.s, pWROTO->getImgSize());
  }

  if (pWDMAO != NULL) {
    qPTempGen.addOutput(PORT_WDMAO, 0);
    qPTempGen.addCrop(eCROP_WDMA, enqueData.mWDMAO.mCropRegion.p,
                      enqueData.mWDMAO.mCropRegion.s, pWDMAO->getImgSize());
  }

  ret = qPTempGen.generate(&qParam) ? OK : BAD_VALUE;
  CHECK_AND_RELEASE(ret == OK, "fail to create QParams");

  // 4.2 QParam filler
  QParamTemplateFiller qParamFiller(&qParam);
  qParamFiller.insertInputBuf(iFrameNum, PORT_IMGI, pIMGI);
  qParamFiller.insertInputBuf(iFrameNum, PORT_TUNING, tunning.get());

  if (!enqueData.mScaleUp && !enqueData.mResized &&
      tuningParam.pLsc2Buf != NULL) {
#if MTK_DP_ENABLE
    if (mDipVer == EDIPHWVersion_50) {
      qParamFiller.insertInputBuf(
          iFrameNum, PORT_IMGCI,
          static_cast<IImageBuffer*>(tuningParam.pLsc2Buf));
    } else if (mDipVer == EDIPHWVersion_40) {
      qParamFiller.insertInputBuf(
          iFrameNum, PORT_DEPI,
          static_cast<IImageBuffer*>(tuningParam.pLsc2Buf));
    }
#else
    qParamFiller.insertInputBuf(
        iFrameNum, PORT_IMGCI,
        static_cast<IImageBuffer*>(tuningParam.pLsc2Buf));
#endif
  }

  if (!enqueData.mScaleUp && !enqueData.mResized && pLCEI != NULL) {
    qParamFiller.insertInputBuf(iFrameNum, PORT_LCEI, pLCEI);
    if (pPackage->mpModuleInfo != NULL) {
      qParamFiller.insertInputBuf(iFrameNum, PORT_DEPI, pLCEI);
    }
  }

  if (!enqueData.mScaleUp && tuningParam.pBpc2Buf != NULL) {
    qParamFiller.insertInputBuf(
        iFrameNum, PORT_IMGBI,
        static_cast<IImageBuffer*>(tuningParam.pBpc2Buf));
  }

  if (pIMG2O != NULL) {
    qParamFiller.insertOutputBuf(iFrameNum, PORT_IMG2O, pIMG2O);
  }

  if (pIMG3O != NULL) {
    qParamFiller.insertOutputBuf(iFrameNum, PORT_IMG3O, pIMG3O);
  }

  if (pWROTO != NULL) {
    qParamFiller.insertOutputBuf(iFrameNum, PORT_WROTO, pWROTO);
  }

  if (pWDMAO != NULL) {
    qParamFiller.insertOutputBuf(iFrameNum, PORT_WDMAO, pWDMAO);
  }

  qParamFiller.setInfo(iFrameNum, requestNo, requestNo, enqueData.mTaskId);

  ret = qParamFiller.validate() ? OK : BAD_VALUE;
  CHECK_AND_RELEASE(ret == OK, "fail to create QParamFiller");

  // 5. prepare rest buffers using mdp copy
  auto IsFovCovered = [](P2Output& rSrc, MDPOutput& rDst) {
    if (rSrc.mpBuf == NULL || rDst.mpBuf == NULL) {
      return MFALSE;
    }

    MSize& srcCrop = rSrc.mCropRegion.s;
    MSize& dstCrop = rDst.mCropRegion.s;
    // Make sure that the source FOV covers the destination FOV
    if (srcCrop.w < dstCrop.w || srcCrop.h < dstCrop.h) {
      return MFALSE;
    }

    MSize srcSize = rSrc.mpBuf->getImgSize();
    MSize dstSize = rDst.mpBuf->getImgSize();
    if (rSrc.mTrans & eTransform_ROT_90) {
      swap(srcSize.w, srcSize.h);
    }

    if (rDst.mTrans & eTransform_ROT_90) {
      swap(dstSize.w, dstSize.h);
    }

    // Make sure that the source image is bigger than destination image
    if (srcSize.w < dstSize.w || srcSize.h < dstSize.h) {
      return MFALSE;
    }

    simpleTransform tranCrop2SrcBuf(rSrc.mCropRegion.p, rSrc.mCropRegion.s,
                                    srcSize);
    MRect& rCropRegion = rDst.mSourceCrop;
    rCropRegion = transform(tranCrop2SrcBuf, rDst.mCropRegion);
    if (rSrc.mTrans & eTransform_ROT_90) {
      swap(rCropRegion.p.x, rCropRegion.p.y);
      swap(rCropRegion.s.w, rCropRegion.s.h);
    }

    rDst.mpSource = rSrc.mpBuf;

    if ((rSrc.mTrans & rDst.mTrans) == rSrc.mTrans) {
      rDst.mSourceTrans = rDst.mTrans ^ rSrc.mTrans;
    } else {
      rDst.mSourceTrans = 0;
      MUINT32 diff = rDst.mTrans ^ rSrc.mTrans;
      if (diff & eTransform_FLIP_H) {
        rDst.mSourceTrans |= eTransform_FLIP_H;
      }
      if (diff & eTransform_FLIP_V) {
        rDst.mSourceTrans |= eTransform_FLIP_V;
      }
      if (diff & eTransform_ROT_90) {
        rDst.mSourceTrans |= eTransform_ROT_270;
      }
    }

    return MTRUE;
  };

  // select a buffer source for MDP copy
  P2Output* pFirstHit = NULL;
  if (pCopy1 != NULL) {
    if (IsFovCovered(enqueData.mIMG2O, enqueData.mCopy1)) {
      pFirstHit = &enqueData.mIMG2O;
    } else if (IsFovCovered(enqueData.mWDMAO, enqueData.mCopy1)) {
      pFirstHit = &enqueData.mWDMAO;
    } else if (IsFovCovered(enqueData.mWROTO, enqueData.mCopy1)) {
      pFirstHit = &enqueData.mWROTO;
    } else {
      MY_LOGE("Copy1's FOV is not covered by P2 first-run output");
    }
  }

  if (pCopy2 != NULL) {
    if (pFirstHit != NULL && IsFovCovered(*pFirstHit, enqueData.mCopy2)) {
      MY_LOGD("Use the same output to server two MDP outputs");
    } else if (pFirstHit != &enqueData.mIMG2O &&
               IsFovCovered(enqueData.mIMG2O, enqueData.mCopy2)) {
      MY_LOGD("Use different output to server two MDP outputs");
    } else if (pFirstHit != &enqueData.mWDMAO &&
               IsFovCovered(enqueData.mWDMAO, enqueData.mCopy2)) {
      MY_LOGD("Use different output to server two MDP outputs");
    } else if (pFirstHit != &enqueData.mWROTO &&
               IsFovCovered(enqueData.mWROTO, enqueData.mCopy2)) {
      MY_LOGD("Use different output to server two MDP outputs");
    } else {
      MY_LOGE("Copy2's FOV is not covered by P2 first-run output");
    }
  }

  // 6.enque
  pPackage->start();

  // callbacks
  qParam.mpfnCallback = onP2SuccessCallback;
  qParam.mpfnEnQFailCallback = onP2FailedCallback;
  qParam.mpCookie = reinterpret_cast<MVOID*>(pPackage);

  // p2 enque
  if (master && !enqueData.mYuvRep) {
    ret = mpP2Opt->enque(&qParam, LOG_TAG);
  } else if (master && enqueData.mYuvRep) {
    ret = mpP2ReqOpt->enque(&qParam, LOG_TAG);
  } else {
    ret = mpP2Opt2->enque(&qParam, LOG_TAG);
  }

  CHECK_AND_RELEASE(ret == OK, "fail to enque P2");

  TRACE_FUNC_EXIT();
  return MTRUE;

#undef CHECK_AND_RELEASE
}

/***********************************************************
 *
 **********************************************************/
MVOID
P2ANode::calculateSourceCrop(MRect* rSrcCrop,
                             std::shared_ptr<IImageBuffer> src,
                             std::shared_ptr<IImageBuffer> dst,
                             MINT32 dstTrans) {
  MRect srcCrop_origin = *rSrcCrop;
  MSize dstSize = dst->getImgSize();
  if (dstTrans & eTransform_ROT_90) {
    swap(dstSize.w, dstSize.h);
  }

  MSize srcSize = src->getImgSize();
  if (srcCrop_origin.s.w > srcSize.w) {
    MY_LOGW("crop width exceed src width, changed to source width");
    srcCrop_origin.s.w = srcSize.w;
  }
  if (srcCrop_origin.s.h > srcSize.h) {
    MY_LOGW("crop height exceed src height, changed to source height");
    srcCrop_origin.s.h = srcSize.h;
  }

#define THRESHOLD (0.01)
  float ratioSrc = static_cast<float>(srcSize.w / srcSize.h);
  float ratioDst = static_cast<float>(dstSize.w / dstSize.h);
  float ratioDiff = std::abs(ratioDst - ratioSrc);
  MBOOL isSameRatio = (ratioDiff < THRESHOLD);

  MY_LOGD("src ratio(%f), dst ratio(%f), diff(%f) thres(%f) isSameRatio(%d)",
          ratioSrc, ratioDst, ratioDiff, THRESHOLD, isSameRatio);
#undef THRESHOLD

  if (!isSameRatio) {
    // calculate the required image hight according to image ratio
    const double OUTPUT_RATIO =
        (static_cast<double>(dstSize.w)) / (static_cast<double>(dstSize.h));
    rSrcCrop->s = srcCrop_origin.s;
    rSrcCrop->s.h = APPLY_2_ALIGN(rSrcCrop->s.w / OUTPUT_RATIO);
    if (rSrcCrop->s.h > srcCrop_origin.s.h) {
      rSrcCrop->s.h = APPLY_2_ALIGN(srcCrop_origin.s.h);
      rSrcCrop->s.w = APPLY_2_ALIGN(rSrcCrop->s.h * OUTPUT_RATIO);
    } else {
      rSrcCrop->s.w = APPLY_2_ALIGN(srcCrop_origin.s.w);
    }
    rSrcCrop->p.x = (srcCrop_origin.s.w - rSrcCrop->s.w) / 2;
    rSrcCrop->p.y = (srcCrop_origin.s.h - rSrcCrop->s.h) / 2;
  }

  MY_LOGD(
      "srcSize(%dx%d), dstSize(%dx%d), dstTrans(%d) isSameRatio(%d) "
      "crop(%d,%d,%dx%d)->(%d,%d,%dx%d)",
      srcSize.w, srcSize.h, dstSize.w, dstSize.h, dstTrans, isSameRatio,
      srcCrop_origin.p.x, srcCrop_origin.p.y, srcCrop_origin.s.w,
      srcCrop_origin.s.h, rSrcCrop->p.x, rSrcCrop->p.y, rSrcCrop->s.w,
      rSrcCrop->s.h);
}

MBOOL P2ANode::onThreadStart() {
  TRACE_FUNC_ENTER();

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::onThreadStop() {
  TRACE_FUNC_ENTER();

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::onData(DataID id, const RequestPtr& pRequest) {
  TRACE_FUNC_ENTER();
  MY_LOGD_IF(mLogLevel, "Frame %d: %s arrived", pRequest->getRequestNo(),
             PathID2Name(id));
  MBOOL ret = MTRUE;

  if (pRequest->isSatisfied(mNodeId)) {
    mRequests.enque(pRequest);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL P2ANode::onThreadLoop() {
  TRACE_FUNC("Waitloop");
  RequestPtr pRequest;

  CAM_TRACE_CALL();

  if (!waitAllQueue()) {
    return MFALSE;
  }

  if (!mRequests.deque(&pRequest)) {
    MY_LOGE("Request deque out of sync");
    return MFALSE;
  } else if (pRequest == NULL) {
    MY_LOGE("Request out of sync");
    return MFALSE;
  }

  TRACE_FUNC_ENTER();

  pRequest->mTimer.startP2A();
  onRequestProcess(pRequest);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
MVOID
P2ANode::onP2SuccessCallback(QParams* pParams) {
  EnquePackage* pPackage = reinterpret_cast<EnquePackage*>((pParams->mpCookie));

  P2EnqueData& rData = *pPackage->mpEnqueData.get();
  P2ANode* pNode = pPackage->mpNode;

  if (!rData.mYuvRep) {
    pNode->mpP2Opt->putTuningBuffer(pPackage->mTuningData);
  } else {
    pNode->mpP2ReqOpt->putTuningBuffer(pPackage->mTuningData);
  }
  pPackage->stop();

  MY_LOGI("R/F Num: %d/%d, task:%d, timeconsuming: %dms", rData.mRequestNo,
          rData.mFrameNo, rData.mTaskId, pPackage->getElapsed());

  if (rData.mDebugDump) {
    char filename[256] = {0};
    FILE_DUMP_NAMING_HINT hint;
    hint.UniqueKey = rData.mUniqueKey;
    hint.RequestNo = rData.mRequestNo;
    hint.FrameNo = rData.mFrameNo;

    extract_by_SensorOpenId(&hint, rData.mSensorId);

    auto DumpYuvBuffer = [&](IImageBuffer* pImgBuf, YUV_PORT port,
                             const char* pStr) -> MVOID {
      if (pImgBuf == NULL) {
        return;
      }

      extract(&hint, pImgBuf);
      if (pStr == NULL) {
        genFileName_YUV(filename, sizeof(filename), &hint, port);
      } else {
        genFileName_YUV(filename, sizeof(filename), &hint, port, pStr);
      }

      pImgBuf->saveToFile(filename);
      MY_LOGD("Dump YUV: %s", filename);
    };

    auto DumpLcsBuffer = [&](IImageBuffer* pImgBuf, const char* pStr) -> MVOID {
      if (pImgBuf == NULL) {
        return;
      }

      extract(&hint, pImgBuf);
      genFileName_LCSO(filename, sizeof(filename), &hint, pStr);
      pImgBuf->saveToFile(filename);
      MY_LOGD("Dump LCEI: %s", filename);
    };

    auto DumpRawBuffer = [&](IImageBuffer* pImgBuf, RAW_PORT port,
                             const char* pStr) -> MVOID {
      if (pImgBuf == NULL) {
        return;
      }

      extract(&hint, pImgBuf);
      genFileName_RAW(filename, sizeof(filename), &hint, port, pStr);
      pImgBuf->saveToFile(filename);
      MY_LOGD("Dump RAW: %s", filename);
    };

    std::string str;
    if (rData.mEnableMFB) {
      DumpYuvBuffer(rData.mIMG3O.mpBuf, YUV_PORT_IMG3O, NULL);
      DumpYuvBuffer(rData.mIMG2O.mpBuf, YUV_PORT_IMG2O, NULL);

      // do NOT show sensor name for MFNR naming
      hint.SensorDev = -1;

      MINT32 iso = 0;
      MINT64 exp = 0;
      tryGetMetadata<MINT32>(rData.mpIMetaDynamic, MTK_SENSOR_SENSITIVITY,
                             &iso);
      tryGetMetadata<MINT64>(rData.mpIMetaDynamic, MTK_SENSOR_EXPOSURE_TIME,
                             &exp);

      // convert ns into us
      exp /= 1000;
      str =
          base::StringPrintf("mfll-iso-%d-exp-%" PRId64 "-bfbld-yuv", iso, exp);
      DumpYuvBuffer(rData.mWDMAO.mpBuf, YUV_PORT_NULL, str.c_str());
      str = base::StringPrintf("mfll-iso-%d-exp-%" PRId64 "-bfbld-qyuv", iso,
                               exp);
      DumpYuvBuffer(rData.mWROTO.mpBuf, YUV_PORT_NULL, str.c_str());

      IImageBuffer* pLCEI = rData.mLCEI.mpBuf;
      if (pLCEI != NULL) {
        str = base::StringPrintf(
            "mfll-iso-%d-exp-%" PRId64 "-bfbld-lcso__%dx%d", iso, exp,
            pLCEI->getImgSize().w, pLCEI->getImgSize().h);
        DumpLcsBuffer(pLCEI, str.c_str());
      }

      str =
          base::StringPrintf("mfll-iso-%d-exp-%" PRId64 "-bfbld-raw", iso, exp);
      DumpRawBuffer(rData.mIMGI.mpBuf, RAW_PORT_NULL, str.c_str());
    } else {
      MUINT32 run = rData.mScaleUp ? 2 : 1;

      const char* pStr = NULL;
      if (run > 1) {
        str = base::StringPrintf("run%dout", run);
        pStr = str.c_str();
      }

      DumpYuvBuffer(rData.mIMG3O.mpBuf, YUV_PORT_IMG3O, pStr);
      DumpYuvBuffer(rData.mIMG2O.mpBuf, YUV_PORT_IMG2O, pStr);
      DumpYuvBuffer(rData.mWDMAO.mpBuf, YUV_PORT_WDMAO, pStr);
      DumpYuvBuffer(rData.mWROTO.mpBuf, YUV_PORT_WROTO, pStr);

      if (run < 2) {
        DumpLcsBuffer(rData.mLCEI.mpBuf, NULL);
        if (!rData.mYuvRep) {
          DumpRawBuffer(rData.mIMGI.mpBuf,
                        rData.mResized ? RAW_PORT_RRZO : RAW_PORT_IMGO, NULL);
        }
      }
    }
  }
  MBOOL hasCopyTask = MFALSE;
  if (rData.mCopy1.mpBuf != nullptr || rData.mCopy2.mpBuf != nullptr) {
    auto pTaskQueue = pNode->mTaskQueue.get();
    if (pTaskQueue != nullptr) {
      std::function<void()> task = [pPackage]() {
        copyBuffers(pPackage);
        delete pPackage;
      };

      pTaskQueue->addTask(task);
      hasCopyTask = MTRUE;
    }
  }

  if (!hasCopyTask) {
    delete pPackage;
  }
}

/***************************************************************************
 *
 ****************************************************************************/
MVOID
P2ANode::onP2FailedCallback(QParams* pParams) {
  EnquePackage* pPackage = reinterpret_cast<EnquePackage*>((pParams->mpCookie));

  P2EnqueData& rData = *pPackage->mpEnqueData.get();

  pPackage->stop();

  MY_LOGI("R/F Num: %d/%d, task:%d, timeconsuming: %dms", rData.mRequestNo,
          rData.mFrameNo, rData.mTaskId, pPackage->getElapsed());

  // TODO(MTK): check it
  // pNode->handleData(ERROR_OCCUR_NOTIFY, pPackage->mPImgInfo);
  delete pPackage;
}

/**********************************************************************
 *
 **********************************************************************/
MBOOL
P2ANode::onRequestProcess(RequestPtr& pRequest) {
  MINT32 requestNo = pRequest->getRequestNo();
  MINT32 frameNo = pRequest->getFrameNo();

#ifdef GTEST
  MY_LOGD("run GTEST, return directly, request:%d", requestNo);
  dispatch(pRequest);
  return MTRUE;
#endif

  CAM_TRACE_FMT_BEGIN("p2a:process|r%df%d", requestNo, frameNo);
  MY_LOGI("+, R/F Num: %d/%d", requestNo, frameNo);

  auto pNodeReq = pRequest->getNodeRequest(NID_P2A);
  MBOOL ret;

  // 0. Create request holder
  incExtThreadDependency();
  std::shared_ptr<RequestHolder> pHolder(new RequestHolder(),
                                         [=](RequestHolder* p) mutable {
                                           delete p;
                                           onRequestFinish(pRequest);
                                           decExtThreadDependency();
                                         });

  // 0-1. Acquire Metadata
  IMetadata* pIMetaDynamic = pNodeReq->acquireMetadata(MID_MAIN_IN_P1_DYNAMIC);
  IMetadata* pIMetaApp = pNodeReq->acquireMetadata(MID_MAIN_IN_APP);
  IMetadata* pIMetaHal = pNodeReq->acquireMetadata(MID_MAIN_IN_HAL);
  IMetadata* pOMetaApp = pNodeReq->acquireMetadata(MID_MAIN_OUT_APP);
  IMetadata* pOMetaHal = pNodeReq->acquireMetadata(MID_MAIN_OUT_HAL);

  IMetadata* pIMetaHal2 = NULL;
  IMetadata* pIMetaDynamic2 = NULL;
  if (hasSubSensor()) {
    pIMetaHal2 = pNodeReq->acquireMetadata(MID_SUB_IN_HAL);
    pIMetaDynamic2 = pNodeReq->acquireMetadata(MID_SUB_IN_P1_DYNAMIC);
  }

  // 0-2. Get Data
  MINT32 uniqueKey = 0;
  tryGetMetadata<MINT32>(pIMetaHal, MTK_PIPELINE_UNIQUE_KEY, &uniqueKey);

  MINT32 iIsoValue = 0;
  tryGetMetadata<MINT32>(pIMetaDynamic, MTK_SENSOR_SENSITIVITY, &iIsoValue);

  // 1. Full RAW of main sensor
  // YUV Reprocessing
  MBOOL isYuvRep =
      pNodeReq->mapBufferID(TID_MAIN_FULL_YUV, INPUT) != NULL_BUFFER;
  // Down Scale: Only for IMGO
  MINT32 iDSRatio = 1;
  IImageBuffer* pDownScaleBuffer = nullptr;
  MSize fullSize;
  MSize downSize;

  MBOOL isRunDS = MFALSE;

  if (isYuvRep || pRequest->getParameter(PID_FRAME_COUNT) > 1) {
    // do NOT execute down-scale if multi-frame blending or YUV reprocessing
  } else if (mDebugDS == OFF) {
    // do NOT execute down-scale if force DS off
  } else if (mDebugDS == ON) {
    iDSRatio = mDebugDSRatio;
    isRunDS = MTRUE;
  } else {
    isRunDS = 0;  // (1600 <= iIsoValue);
  }
  // 1-1. Downscale
  if (isRunDS) {
    std::shared_ptr<P2EnqueData> pEnqueData = std::make_shared<P2EnqueData>();
    P2EnqueData& rEnqueData = *pEnqueData.get();
    rEnqueData.mpHolder = pHolder;
    rEnqueData.mIMGI.mBufId = pNodeReq->mapBufferID(TID_MAIN_FULL_RAW, INPUT);
    rEnqueData.mIMGI.mpBuf = pNodeReq->acquireBuffer(rEnqueData.mIMGI.mBufId);
    rEnqueData.mLCEI.mBufId = pNodeReq->mapBufferID(TID_MAIN_LCS, INPUT);
    rEnqueData.mpIMetaApp = pIMetaApp;
    rEnqueData.mpIMetaHal = pIMetaHal;

    fullSize = rEnqueData.mIMGI.mpBuf->getImgSize();
    downSize = MSize(fullSize.w / iDSRatio, fullSize.h / iDSRatio);
    MY_LOGD("apply down-scale denoise: (%dx%d) -> (%dx%d)", fullSize.w,
            fullSize.h, downSize.w, downSize.h);

    // Get working buffer
    SmartImageBuffer pWorkingBuffer =
        mpBufferPool->getImageBuffer(downSize.w, downSize.h, eImgFmt_YUY2);
    // Push to resource holder
    pHolder->mpBuffers.push_back(pWorkingBuffer);
    pDownScaleBuffer = pWorkingBuffer->mImageBuffer.get();
    rEnqueData.mWDMAO.mpBuf = pDownScaleBuffer;

    rEnqueData.mSensorId = mSensorIndex;
    rEnqueData.mUniqueKey = uniqueKey;
    rEnqueData.mRequestNo = requestNo;
    rEnqueData.mFrameNo = frameNo;
    rEnqueData.mTaskId = 0;
    ret = enqueISP(pRequest, pEnqueData);

    if (!ret) {
      MY_LOGE("main sensor: downsize failed!");
      return MFALSE;
    }
  }

  // 1-2. Upscale or Full-size enque
  {
    std::shared_ptr<P2EnqueData> pEnqueData = std::make_shared<P2EnqueData>();
    P2EnqueData& rEnqueData = *pEnqueData.get();
    rEnqueData.mpHolder = pHolder;
    MBOOL isPureRaw = MFALSE;
    MSize srcSize;
    if (isRunDS) {
      rEnqueData.mIMGI.mpBuf = pDownScaleBuffer;
      rEnqueData.mScaleUp = MTRUE;
      rEnqueData.mScaleUpSize = fullSize;
      srcSize = downSize;
    } else {
      if (isYuvRep) {
        rEnqueData.mIMGI.mBufId =
            pNodeReq->mapBufferID(TID_MAIN_FULL_YUV, INPUT);
        rEnqueData.mYuvRep = MTRUE;
      } else {
        rEnqueData.mIMGI.mBufId =
            pNodeReq->mapBufferID(TID_MAIN_FULL_RAW, INPUT);
      }

      rEnqueData.mIMGI.mpBuf = pNodeReq->acquireBuffer(rEnqueData.mIMGI.mBufId);

      MUINT sensorDev = 0;
      IHalSensorList* const pIHalSensorList = GET_HalSensorList();
      if (pIHalSensorList) {
        sensorDev =
            (MUINT32)pIHalSensorList->querySensorDevIdx(mSensorIndex);
        NSCam::SensorStaticInfo sensorStaticInfo;
        memset(&sensorStaticInfo, 0, sizeof(NSCam::SensorStaticInfo));
        pIHalSensorList->querySensorStaticInfo(sensorDev, &sensorStaticInfo);
        auto pHeap = rEnqueData.mIMGI.mpBuf->getImageBufferHeap();
        if (pHeap != nullptr && pHeap->getColorArrangement() < 0) {
          pHeap->setColorArrangement(
              (MINT32)sensorStaticInfo.sensorFormatOrder);
          MY_LOGD("set ColorArrangement %d",
                  sensorStaticInfo.sensorFormatOrder);
        }
      }

      isPureRaw = MFALSE;
      rEnqueData.mIMGI.mPureRaw = isPureRaw;
      rEnqueData.mLCEI.mBufId = pNodeReq->mapBufferID(TID_MAIN_LCS, INPUT);
      srcSize = rEnqueData.mIMGI.mpBuf->getImgSize();
    }

    // the larger size has higher priority, the smaller size could using larger
    // image to copy via MDP
    const TypeID_T typeIds[] = {TID_MAIN_FULL_YUV,  TID_JPEG,
                                TID_MAIN_CROP1_YUV, TID_MAIN_CROP2_YUV,
                                TID_MAIN_SPEC_YUV,  TID_MAIN_FD_YUV,
                                TID_POSTVIEW,       TID_THUMBNAIL};

    MBOOL hasP2Cropper = !mISP3_0 || !isPureRaw;
    for (TypeID_T typeId : typeIds) {
      BufferID_T bufId = pNodeReq->mapBufferID(typeId, OUTPUT);
      if (bufId == NULL_BUFFER) {
        continue;
      }

      if ((typeId == TID_MAIN_FULL_YUV) && (mHasAllocDip == MFALSE)) {
        MSize size = pRequest->getImageSize(bufId);
        MINT format = pRequest->getImageFormat(bufId);
        MBOOL bUseSingleBuffer = format == eImgFmt_I422;
        PoolKey_T poolKey = std::make_tuple(size.w, size.h, format);
        auto pImagePool = ImageBufferPool::create(
            "CapturePipe", size.w, size.h, (EImageFormat)format,
            ImageBufferPool::USAGE_HW_AND_SW, bUseSingleBuffer);
        if (pImagePool != nullptr) {
          mpP2Opt->requsetCapBuffer(NSImageio::NSIspio::EPortIndex_WDMAO,
                                    size.w, size.h, format,
                                    CAPTURE_CACHE_BUFFER_NUM, &mDipBuffers);
          for (auto& it : mDipBuffers) {
            pImagePool->add(it);
          }
          mHasAllocDip = MTRUE;
          mpBufferPool->addToPool(poolKey, pImagePool);
        } else {
          MY_LOGE("create buffer pool failed!");
        }
      }

      IImageBuffer* pBuf = pNodeReq->acquireBuffer(bufId);
      if (pBuf == nullptr) {
        MY_LOGE("should not have null buffer. type:%d, buf:%d", typeId, bufId);
        continue;
      }

      MUINT32 trans = pNodeReq->getImageTransform(bufId);
      MBOOL needCrop = typeId == TID_JPEG || typeId == TID_MAIN_CROP1_YUV ||
                       typeId == TID_MAIN_CROP2_YUV || typeId == TID_POSTVIEW ||
                       typeId == TID_THUMBNAIL;

      auto UsedOutput = [&](P2Output& o) -> MBOOL {
        return o.mBufId != NULL_BUFFER;
      };

      auto SetOutput = [&](P2Output& o) {
        o.mpBuf = pBuf;
        o.mBufId = bufId;
        o.mHasCrop = needCrop;
        o.mTrans = trans;
      };

      // Use P2 resizer to serve FD or thumbnail buffer,
      // but do NOT use IMG2O to crop in ISP 3.0 while enque pure raw
      if (!UsedOutput(rEnqueData.mIMG2O) &&
          (typeId == TID_MAIN_FD_YUV || typeId == TID_THUMBNAIL) &&
          (hasP2Cropper || !needCrop)) {
        SetOutput(rEnqueData.mIMG2O);
      } else if (typeId == TID_MAIN_FULL_YUV && mForceImg3o) {
        SetOutput(rEnqueData.mIMG3O);
      } else if (!UsedOutput(rEnqueData.mWDMAO) && trans == 0) {
        SetOutput(rEnqueData.mWDMAO);
      } else if (!UsedOutput(rEnqueData.mWROTO)) {
        SetOutput(rEnqueData.mWROTO);
      } else if (!UsedOutput(rEnqueData.mCopy1)) {
        SetOutput(rEnqueData.mCopy1);
      } else if (!UsedOutput(rEnqueData.mCopy2)) {
        SetOutput(rEnqueData.mCopy2);
      } else {
        MY_LOGE("the buffer is not served, type:%s", TypeID2Name(typeId));
      }
    }

    if ((rEnqueData.mIMG2O.mBufId & rEnqueData.mWROTO.mBufId &
         rEnqueData.mWDMAO.mBufId & rEnqueData.mIMG3O.mBufId) != NULL_BUFFER) {
      rEnqueData.mpIMetaDynamic = pIMetaDynamic;
      rEnqueData.mpIMetaApp = pIMetaApp;
      rEnqueData.mpIMetaHal = pIMetaHal;
      rEnqueData.mpOMetaApp = pOMetaApp;
      rEnqueData.mpOMetaHal = pOMetaHal;
      rEnqueData.mSensorId = mSensorIndex;
      rEnqueData.mUniqueKey = uniqueKey;
      rEnqueData.mRequestNo = requestNo;
      rEnqueData.mFrameNo = frameNo;

      if (!mISP3_0 && mDebugImg3o) {
        SmartImageBuffer pDebugBuffer =
            mpBufferPool->getImageBuffer(srcSize.w, srcSize.h, eImgFmt_YUY2);
        pHolder->mpBuffers.push_back(pDebugBuffer);
        rEnqueData.mIMG3O.mpBuf = pDebugBuffer->mImageBuffer.get();
      }

      ret = enqueISP(pRequest, pEnqueData);

      if (!ret) {
        MY_LOGE("enqueISP failed!");
        return MFALSE;
      }
    }
  }

  // 2. Full RAW of sub sensor
  if (hasSubSensor()) {
    BufferID_T uOBufId = pNodeReq->mapBufferID(TID_SUB_FULL_YUV, OUTPUT);

    if (uOBufId != NULL_BUFFER) {
      shared_ptr<P2EnqueData> pEnqueData = make_shared<P2EnqueData>();
      P2EnqueData& rEnqueData = *pEnqueData.get();

      rEnqueData.mpHolder = pHolder;
      rEnqueData.mIMGI.mBufId = pNodeReq->mapBufferID(TID_SUB_FULL_RAW, INPUT);
      rEnqueData.mLCEI.mBufId = pNodeReq->mapBufferID(TID_SUB_LCS, INPUT);
      rEnqueData.mWDMAO.mBufId = uOBufId;
      rEnqueData.mpIMetaApp = pIMetaApp;
      rEnqueData.mpIMetaHal = pIMetaHal2;
      rEnqueData.mUniqueKey = uniqueKey;
      rEnqueData.mRequestNo = requestNo;
      rEnqueData.mFrameNo = frameNo;
      rEnqueData.mTaskId = 2;
      ret = enqueISP(pRequest, pEnqueData);

      if (!ret) {
        MY_LOGE("sub sensor: full yuv failed!");
        return MFALSE;
      }
    }
  }

  // 3. Resized RAW of main sensor
  {
    BufferID_T uOBufId = pNodeReq->mapBufferID(TID_MAIN_RSZ_YUV, OUTPUT);
    if (uOBufId != NULL_BUFFER) {
      shared_ptr<P2EnqueData> pEnqueData = make_shared<P2EnqueData>();
      P2EnqueData& rEnqueData = *pEnqueData.get();

      rEnqueData.mpHolder = pHolder;
      rEnqueData.mIMGI.mBufId = pNodeReq->mapBufferID(TID_MAIN_RSZ_RAW, INPUT);
      rEnqueData.mWDMAO.mBufId = uOBufId;
      rEnqueData.mWDMAO.mBufId = uOBufId;
      rEnqueData.mpIMetaApp = pIMetaApp;
      rEnqueData.mpIMetaHal = pIMetaHal;
      rEnqueData.mSensorId = mSensorIndex;
      rEnqueData.mResized = MTRUE;
      rEnqueData.mUniqueKey = uniqueKey;
      rEnqueData.mRequestNo = requestNo;
      rEnqueData.mFrameNo = frameNo;
      rEnqueData.mTaskId = 3;
      ret = enqueISP(pRequest, pEnqueData);

      if (!ret) {
        MY_LOGE("main sensor: resized yuv failed!");
        return MFALSE;
      }
    }
  }
  // 4. Resized RAW of sub sensor
  if (hasSubSensor()) {
    BufferID_T uOBufId = pNodeReq->mapBufferID(TID_SUB_RSZ_YUV, OUTPUT);

    if (uOBufId != NULL_BUFFER) {
      shared_ptr<P2EnqueData> pEnqueData = make_shared<P2EnqueData>();
      P2EnqueData& rEnqueData = *pEnqueData.get();

      rEnqueData.mpHolder = pHolder;
      rEnqueData.mIMGI.mBufId = pNodeReq->mapBufferID(TID_SUB_RSZ_RAW, INPUT);
      rEnqueData.mWDMAO.mBufId = uOBufId;
      rEnqueData.mpIMetaApp = pIMetaApp;
      rEnqueData.mpIMetaHal = pIMetaHal2;
      rEnqueData.mResized = MTRUE;
      rEnqueData.mUniqueKey = uniqueKey;
      rEnqueData.mRequestNo = requestNo;
      rEnqueData.mFrameNo = frameNo;
      rEnqueData.mTaskId = 4;
      ret = enqueISP(pRequest, pEnqueData);

      if (!ret) {
        MY_LOGE("sub sensor: resized yuv failed!");
        return MFALSE;
      }
    }
  }

  MY_LOGD("-, R/F Num: %d/%d", requestNo, frameNo);
  CAM_TRACE_FMT_END();
  return MTRUE;
}

/*****************************************************************************
 *
 *****************************************************************************/
MBOOL P2ANode::copyBuffers(EnquePackage* pPackage) {
  P2EnqueData& rData = *pPackage->mpEnqueData.get();
  MINT32 requestNo = rData.mRequestNo;
  MINT32 frameNo = rData.mFrameNo;
  CAM_TRACE_FMT_BEGIN("p2a:copy|r%df%d", requestNo, frameNo);
  MY_LOGD("+, R/F Num: %d/%d", requestNo, frameNo);

  IImageBuffer* pSource1 = rData.mCopy1.mpSource;
  IImageBuffer* pSource2 = rData.mCopy2.mpSource;
  IImageBuffer* pCopy1 = rData.mCopy1.mpBuf;
  IImageBuffer* pCopy2 = rData.mCopy2.mpBuf;
  MBOOL hasCopy2 = pCopy2 != NULL;
  MBOOL hasSameSrc = hasCopy2 ? pSource1 == pSource2 : MFALSE;

  if (pSource1 == nullptr || pCopy1 == nullptr) {
    MY_LOGE("should have source1 & copy1 buffer here. src:%p, dst:%p", pSource1,
            pCopy1);
    return MFALSE;
  }

  if (hasCopy2 && pSource2 == nullptr) {
    MY_LOGE("should have source2 buffer here. src:%p", pSource1);
    return MFALSE;
  }

  std::string strCopyLog;

  auto InputLog = [&](const char* sPort, IImageBuffer* pBuf) mutable {
    strCopyLog += base::StringPrintf(
        "Sensor(%d) Resized(%d) R/F Num: %d/%d, Copy: %s Fmt(0x%x) Size(%dx%d) "
        "VA/PA(%#" PRIxPTR "/%#" PRIxPTR ")",
        rData.mSensorId, rData.mResized, requestNo, frameNo, sPort,
        pBuf->getImgFormat(), pBuf->getImgSize().w, pBuf->getImgSize().h,
        pBuf->getBufVA(0), pBuf->getBufPA(0));
  };

  auto OutputLog = [&](const char* sPort, MDPOutput& rOut) mutable {
    strCopyLog += base::StringPrintf(
        ", %s Rot(%d) Crop(%d,%d)(%dx%d) Size(%dx%d) VA/PA(%#" PRIxPTR
        "/%#" PRIxPTR ")",
        sPort, rOut.mSourceTrans, rOut.mSourceCrop.p.x, rOut.mSourceCrop.p.y,
        rOut.mSourceCrop.s.w, rOut.mSourceCrop.s.h, rOut.mpBuf->getImgSize().w,
        rOut.mpBuf->getImgSize().h, rOut.mpBuf->getBufVA(0),
        rOut.mpBuf->getBufPA(0));
  };

  InputLog("SRC1", pSource1);
  OutputLog("COPY1", rData.mCopy1);

  if (hasCopy2) {
    if (!hasSameSrc) {
      MY_LOGD("%s", strCopyLog.c_str());
      strCopyLog.clear();
      InputLog("SRC2", pSource2);
    }
    OutputLog("COPY2", rData.mCopy2);
  }
  MY_LOGD("%s", strCopyLog.c_str());

  CAM_TRACE_FMT_END();
  MY_LOGD("-, R/F Num: %d/%d", requestNo, frameNo);
  return MTRUE;
}

MVOID P2ANode::onRequestFinish(RequestPtr& pRequest) {
  MINT32 requestNo = pRequest->getRequestNo();
  MINT32 frameNo = pRequest->getFrameNo();
  CAM_TRACE_FMT_BEGIN("p2a::finish|r%df%d)", requestNo, frameNo);
  MY_LOGD("+, R/F Num: %d/%d", requestNo, frameNo);

  pRequest->decNodeReference(NID_P2A);

  if ((pRequest->getParameter(PID_ENABLE_NEXT_CAPTURE) > 0) &&
      (pRequest->getParameter(PID_FRAME_COUNT) < 2 ||
       pRequest->getParameter(PID_FRAME_INDEX) == 0)) {
    if (pRequest->mpCallback != NULL) {
      MY_LOGD("Nofity: next capture");
      pRequest->mpCallback->onContinue(pRequest);
    }
  }

  pRequest->mTimer.stopP2A();
  dispatch(pRequest);

  CAM_TRACE_FMT_END();
  MY_LOGD("-, R/F Num: %d/%d", requestNo, frameNo);
}

P2ANode::EnquePackage::~EnquePackage() {
  if (mpModuleInfo != nullptr) {
    if (mpModuleInfo->moduleStruct != nullptr) {
      delete static_cast<_SRZ_SIZE_INFO_*>(mpModuleInfo->moduleStruct);
    }
    delete mpModuleInfo;
  }
}

MERROR P2ANode::evaluate(CaptureFeatureInferenceData* rInfer) {
  auto& rSrcData = rInfer->getSharedSrcData();
  auto& rDstData = rInfer->getSharedDstData();
  auto& rFeatures = rInfer->getSharedFeatures();
  auto& rMetadatas = rInfer->getSharedMetadatas();

  MBOOL hasMain = MFALSE;
  MBOOL hasSub = MFALSE;

  // Reprocessing
  if (rInfer->hasType(TID_MAIN_FULL_YUV)) {
    rSrcData.emplace_back();
    auto& src_0 = rSrcData.back();
    src_0.mTypeId = TID_MAIN_FULL_YUV;
    src_0.mSizeId = SID_FULL;

    rDstData.emplace_back();
    auto& dst_0 = rDstData.back();
    dst_0.mTypeId = TID_MAIN_FULL_YUV;
    dst_0.mSizeId = SID_FULL;
    dst_0.mSize = rInfer->getSize(TID_MAIN_FULL_YUV);
    dst_0.mFormat = eImgFmt_YV12;

    hasMain = MTRUE;
  } else if (rInfer->hasType(TID_MAIN_FULL_RAW)) {
    rSrcData.emplace_back();
    auto& src_0 = rSrcData.back();
    src_0.mTypeId = TID_MAIN_FULL_RAW;
    src_0.mSizeId = SID_FULL;

    rSrcData.emplace_back();
    auto& src_1 = rSrcData.back();
    src_1.mTypeId = TID_MAIN_LCS;
    src_1.mSizeId = SID_ARBITRARY;

    rDstData.emplace_back();
    auto& dst_0 = rDstData.back();
    dst_0.mTypeId = TID_MAIN_FULL_YUV;
    dst_0.mSizeId = SID_FULL;
    dst_0.mSize = rInfer->getSize(TID_MAIN_FULL_RAW);
    if (mForceImg3o422) {
      dst_0.mFormat = eImgFmt_YUY2;
    } else {
      dst_0.mFormat = eImgFmt_YV12;
    }

    hasMain = MTRUE;
  }

  if (rInfer->hasType(TID_SUB_FULL_RAW)) {
    rSrcData.emplace_back();
    auto& src_0 = rSrcData.back();
    src_0.mTypeId = TID_SUB_FULL_RAW;
    src_0.mSizeId = SID_FULL;

    rSrcData.emplace_back();
    auto& src_1 = rSrcData.back();
    src_1.mTypeId = TID_SUB_LCS;
    src_1.mSizeId = SID_ARBITRARY;

    rDstData.emplace_back();
    auto& dst_0 = rDstData.back();
    dst_0.mTypeId = TID_SUB_FULL_YUV;
    dst_0.mSizeId = SID_FULL;
    dst_0.mSize = rInfer->getSize(TID_SUB_FULL_RAW);
    if (mForceImg3o422) {
      dst_0.mFormat = eImgFmt_YUY2;
    } else {
      dst_0.mFormat = eImgFmt_YV12;
    }
    hasSub = MTRUE;
  }

  if (rInfer->hasType(TID_MAIN_RSZ_RAW)) {
    rSrcData.emplace_back();
    auto& src_0 = rSrcData.back();
    src_0.mTypeId = TID_MAIN_RSZ_RAW;
    src_0.mSizeId = SID_RESIZED;

    rDstData.emplace_back();
    auto& dst_0 = rDstData.back();
    dst_0.mTypeId = TID_MAIN_RSZ_YUV;
    dst_0.mSizeId = SID_RESIZED;
    dst_0.mSize = rInfer->getSize(TID_MAIN_RSZ_RAW);
    dst_0.mFormat = eImgFmt_YV12;

    hasMain = MTRUE;
  }

  if (rInfer->hasType(TID_SUB_RSZ_RAW)) {
    rSrcData.emplace_back();
    auto& src_0 = rSrcData.back();
    src_0.mTypeId = TID_SUB_RSZ_RAW;
    src_0.mSizeId = SID_RESIZED;

    rDstData.emplace_back();
    auto& dst_0 = rDstData.back();
    dst_0.mTypeId = TID_SUB_RSZ_YUV;
    dst_0.mSizeId = SID_RESIZED;
    dst_0.mSize = rInfer->getSize(TID_SUB_RSZ_RAW);
    dst_0.mFormat = eImgFmt_YV12;

    hasSub = MTRUE;
  }

  if (hasMain) {
    rMetadatas.push_back(MID_MAIN_IN_P1_DYNAMIC);
    rMetadatas.push_back(MID_MAIN_IN_APP);
    rMetadatas.push_back(MID_MAIN_IN_HAL);
    rMetadatas.push_back(MID_MAIN_OUT_APP);
    rMetadatas.push_back(MID_MAIN_OUT_HAL);
  }

  if (hasSub) {
    rMetadatas.push_back(MID_SUB_IN_P1_DYNAMIC);
    rMetadatas.push_back(MID_SUB_IN_HAL);
  }

  rInfer->addNodeIO(NID_P2A, rSrcData, rDstData, rMetadatas, rFeatures);

  return OK;
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
