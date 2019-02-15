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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_Util.h"
#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG P2Util
#define P2_TRACE TRACE_P2_UTIL
#include "P2_LogHeader.h"

#include <algorithm>
#include <memory>
#include <utility>

#define toBool(x) (!!(x))

#define FORCE_TEST_MDP 0

namespace P2 {

P2Util::SimpleIO::SimpleIO() : mResized(MFALSE), mUseLMV(MFALSE) {}

MVOID P2Util::SimpleIO::setUseLMV(MBOOL useLMV) {
  mUseLMV = useLMV;
}

MBOOL P2Util::SimpleIO::hasInput() const {
  return isValid(mIMGI);
}

MBOOL P2Util::SimpleIO::hasOutput() const {
  return isValid(mIMG2O) || isValid(mIMG3O) || isValid(mWROTO) ||
         isValid(mWDMAO);
}

MBOOL P2Util::SimpleIO::isResized() const {
  return mResized;
}

MSize P2Util::SimpleIO::getInputSize() const {
  MSize size(0, 0);
  if (isValid(mIMGI)) {
    size = mIMGI->getIImageBufferPtr()->getImgSize();
  }
  return size;
}

MVOID P2Util::SimpleIO::updateResult(MBOOL result) const {
  if (mIMG2O != nullptr) {
    mIMG2O->updateResult(result);
  }
  if (mWROTO != nullptr) {
    mWROTO->updateResult(result);
  }
  if (mWDMAO != nullptr) {
    mWDMAO->updateResult(result);
  }
}

MVOID P2Util::SimpleIO::dropRecord() const {
  if (mWROTO != nullptr && mWROTO->isRecord()) {
    mWROTO->updateResult(MFALSE);
  }
  if (mWDMAO != nullptr && mWDMAO->isRecord()) {
    mWDMAO->updateResult(MFALSE);
  }
}

MVOID P2Util::SimpleIO::earlyRelease(MUINT32 mask, MBOOL result) {
  if (mask & P2Util::RELEASE_DISP) {
    mIMGI = nullptr;
    mLCEI = nullptr;
    if (mWROTO != nullptr && mWROTO->isDisplay()) {
      mWROTO->updateResult(result);
      mWROTO = nullptr;
    }
    if (mWDMAO != nullptr && mWDMAO->isDisplay()) {
      mWDMAO->updateResult(result);
      mWDMAO = nullptr;
    }
  }
  if (mask & P2Util::RELEASE_FD) {
    if (mIMG2O != nullptr) {
      mIMG2O->updateResult(result);
      mIMG2O = nullptr;
    }
  }
}

std::shared_ptr<P2Img> P2Util::SimpleIO::getMDPSrc() const {
  if (mWDMAO != nullptr) {
    return mWDMAO;
  } else if (mWROTO != nullptr) {
    return mWROTO;
  }
  return nullptr;
}

std::shared_ptr<P2Img> P2Util::SimpleIO::getLcso() const {
  return mLCEI;
}

P2IO toP2IO(std::shared_ptr<P2Img> img) {
  P2IO io;
  if (isValid(img)) {
    io.mBuffer = img->getIImageBufferPtr();
    io.mCapability = toCapability(img->getUsage());
    io.mTransform = img->getTransform();
  }
  return io;
}

P2IOPack P2Util::SimpleIO::toP2IOPack() const {
  P2IOPack pack;
  pack.mFlag |= mResized ? P2Flag::FLAG_RESIZED : 0;
  pack.mFlag |= mUseLMV ? P2Flag::FLAG_LMV : 0;

  pack.mIMGI = toP2IO(mIMGI);
  pack.mIMG2O = toP2IO(mIMG2O);
  pack.mWDMAO = toP2IO(mWDMAO);
  pack.mWROTO = toP2IO(mWROTO);

  pack.mLCSO = toP2IO(mLCEI);

  P2IO io;
  io.mBuffer = mTuningBuffer.get();
  pack.mTuning = io;

  return pack;
}

MVOID P2Util::SimpleIO::printIO(const ILog& log) const {
  MY_S_LOGD(log, "resize(%d),lmv(%d)", mResized, mUseLMV);
  MY_S_LOGD(log, "imgi(%d),lcei(%d),img2o(%d),img3o(%d),wroto(%d),wdmao(%d)",
            isValid(mIMGI), isValid(mLCEI), isValid(mIMG2O), isValid(mIMG3O),
            isValid(mWROTO), isValid(mWDMAO));
}

P2Util::SimpleIO P2Util::extractSimpleIO(
    const std::shared_ptr<P2Request>& request, MUINT32 portFlag) {
  ILog log = spToILog(request);
  TRACE_S_FUNC_ENTER(log);
  SimpleIO io;
  MBOOL useVenc = !!(portFlag & P2Util::USE_VENC);
  if (isValid(request->mImg[IN_RESIZED])) {
    io.mResized = MTRUE;
    io.mIMGI = std::move(request->mImg[IN_RESIZED]);
  } else if (isValid(request->mImg[IN_FULL])) {
    io.mResized = MFALSE;
    io.mIMGI = std::move(request->mImg[IN_FULL]);
  }
  if (isValid(request->mImg[IN_LCSO])) {
    io.mLCEI = std::move(request->mImg[IN_LCSO]);
  }
  if (isValid(request->mImg[OUT_FD])) {
    io.mIMG2O = std::move(request->mImg[OUT_FD]);
  }
#if FORCE_TEST_MDP
  io.mWDMAO =
      useVenc ? nullptr : P2Util::extractOut(request, P2Util::FIND_NO_ROTATE);
  if (io.mWDMAO == nullptr) {
    io.mWROTO = P2Util::extractOut(request, P2Util::FIND_ROTATE);
  }
#else
  io.mWROTO = P2Util::extractOut(request, P2Util::FIND_ROTATE);
  if (io.mWROTO == nullptr) {
    io.mWROTO = P2Util::extractOut(request, P2Util::FIND_NO_ROTATE);
  }
  io.mWDMAO =
      useVenc ? nullptr : P2Util::extractOut(request, P2Util::FIND_NO_ROTATE);
#endif  // FORCE_TEST_MDP
  TRACE_S_FUNC_EXIT(log);
  return io;
}

TuningParam P2Util::xmakeTuning(const P2Pack& p2Pack,
                                const SimpleIO& io,
                                std::shared_ptr<IHal3A> hal3A,
                                P2MetaSet* metaSet) {
  const ILog& log = p2Pack.mLog;
  TuningParam tuning;
  MetaSet_T inMetaSet, outMetaSet, *pOutMetaSet = nullptr;

  inMetaSet.MagicNum = 0;
  inMetaSet.appMeta = metaSet->mInApp;
  inMetaSet.halMeta = metaSet->mInHal;
  pOutMetaSet = metaSet->mHasOutput ? &outMetaSet : nullptr;

  tuning = ::NSCam::Feature::P2Util::makeTuningParam(
      log, p2Pack, hal3A, &inMetaSet, pOutMetaSet, io.isResized(),
      io.mTuningBuffer, toIImageBufferPtr(io.mLCEI));

  if (metaSet->mHasOutput) {
    metaSet->mOutApp = outMetaSet.appMeta;
    metaSet->mOutHal = outMetaSet.halMeta;
  }
  return tuning;
}

TuningParam P2Util::xmakeTuning(const P2Pack& p2Pack,
                                const SimpleIn& in,
                                std::shared_ptr<IHal3A> hal3A,
                                P2MetaSet* metaSet) {
  const ILog& log = p2Pack.mLog;
  TuningParam tuning;
  MetaSet_T inMetaSet, outMetaSet, *pOutMetaSet = nullptr;

  inMetaSet.MagicNum = 0;
  inMetaSet.appMeta = metaSet->mInApp;
  inMetaSet.halMeta = metaSet->mInHal;
  pOutMetaSet = metaSet->mHasOutput ? &outMetaSet : nullptr;

  tuning = ::NSCam::Feature::P2Util::makeTuningParam(
      log, p2Pack, hal3A, &inMetaSet, pOutMetaSet, in.isResized(),
      in.mTuningBuffer, toIImageBufferPtr(in.mLCEI));

  if (metaSet->mHasOutput) {
    metaSet->mOutApp = outMetaSet.appMeta;
    metaSet->mOutHal = outMetaSet.halMeta;
  }
  return tuning;
}

QParams P2Util::xmakeQParams(const P2Pack& p2Pack,
                             const SimpleIO& io,
                             const TuningParam& tuning,
                             const P2ObjPtr& p2ObjPtr) {
  QParams qparams;
  qparams = ::NSCam::Feature::P2Util::makeQParams(
      p2Pack, ENormalStreamTag_Prv, io.toP2IOPack(), p2ObjPtr, tuning);

  return qparams;
}

MVOID P2Util::xmakeDpPqParam(const P2Pack& p2Pack,
                             const SimpleOut& out,
                             FD_DATATYPE* const& pfdData) {
  MY_LOGD("Not support DP");
}

MVOID P2Util::releaseTuning(TuningParam* tuning) {
  tuning->pRegBuf = nullptr;
}

std::shared_ptr<P2Img> P2Util::extractOut(
    const std::shared_ptr<P2Request>& request, MUINT32 target) {
  ILog log = spToILog(request);
  TRACE_S_FUNC_ENTER(log);
  std::shared_ptr<P2Img> out;
  MBOOL useRotate = toBool(target & P2Util::FIND_ROTATE);
  MBOOL checkRotate = useRotate != toBool(target & P2Util::FIND_NO_ROTATE);
  MBOOL useDisp = toBool(target & P2Util::FIND_DISP);
  MBOOL useVideo = toBool(target & P2Util::FIND_VIDEO);
  MBOOL checkType = useDisp || useVideo;
  if (request != nullptr) {
    MSize max(0, 0);
    auto maxIt = request->mImgOutArray.end();
    for (auto it = request->mImgOutArray.begin(),
              end = request->mImgOutArray.end();
         it != end; ++it) {
      if (isValid(*it)) {
        if (checkRotate && (useRotate != toBool((*it)->getTransform()))) {
          continue;
        }
        if (checkType && !(useDisp && toBool((*it)->isDisplay())) &&
            !(useVideo && toBool((*it)->isRecord()))) {
          continue;
        }
        MSize size = (*it)->getImgSize();
        if (size.w * size.h > max.w * max.h) {
          max = size;
          maxIt = it;
        }
      }
    }
    if (maxIt != request->mImgOutArray.end()) {
      out = *maxIt;
      (*maxIt) = nullptr;
    }
  }
  TRACE_S_FUNC_EXIT(log);
  return out;
}

P2Util::SimpleIn::SimpleIn(MUINT32 sensorId,
                           std::shared_ptr<P2Request> pRequest)
    : mRequest(pRequest), mSensorId(sensorId) {}

MUINT32 P2Util::SimpleIn::getSensorId() const {
  return mSensorId;
}

MVOID P2Util::SimpleIn::setUseLMV(MBOOL useLMV) {
  mUseLMV = useLMV;
}

MVOID P2Util::SimpleIn::setISResized(MBOOL isResized) {
  mResized = isResized;
}

MBOOL P2Util::SimpleIn::isResized() const {
  return mResized;
}

MBOOL P2Util::SimpleIn::useLMV() const {
  return mUseLMV;
}

MBOOL P2Util::SimpleIn::useCropRatio() const {
  return mUseCropRatio;
}

MSize P2Util::SimpleIn::getInputSize() const {
  return mIMGI->getImgSize();
}

std::shared_ptr<P2Img> P2Util::SimpleIn::getLcso() const {
  return mLCEI;
}

MVOID P2Util::SimpleIn::addCropRatio(const char* name, const float cropRatio) {
  TRACE_FUNC_ENTER();
  mUseCropRatio = MTRUE;
  mCropRatio *= cropRatio;
  TRACE_FUNC("%s cropRatio=%f, total cropRatio=%f", name, cropRatio,
             mCropRatio);
  TRACE_FUNC_EXIT();
}

MBOOL P2Util::SimpleIn::hasCropRatio() const {
  return mUseCropRatio;
}

float P2Util::SimpleIn::getCropRatio() const {
  return mCropRatio;
}

MVOID P2Util::SimpleIn::releaseAllImg() {
  mIMGI = nullptr;
  mLCEI = nullptr;
  mRSSO = nullptr;
  mPreRSSO = nullptr;
}

P2Util::SimpleOut::SimpleOut(MUINT32 sensorId,
                             std::shared_ptr<P2Request> pRequest,
                             std::shared_ptr<P2Img> const& pImg)
    : mRequest(pRequest),
      mDMAConstrainFlag(NSCam::Feature::P2Util::DMACONSTRAIN_2BYTEALIGN |
                        NSCam::Feature::P2Util::DMACONSTRAIN_NOSUBPIXEL),
      mSensorId(sensorId) {
  mImg = std::move(pImg);
}

MUINT32 P2Util::SimpleOut::getSensorId() const {
  return mSensorId;
}

MVOID P2Util::SimpleOut::setIsFD(MBOOL isFDBuffer) {
  mFD = isFDBuffer;
}

MBOOL P2Util::SimpleOut::isDisplay() const {
  return mImg->isDisplay();
}

MBOOL P2Util::SimpleOut::isRecord() const {
  return mImg->isRecord();
}

MBOOL P2Util::SimpleOut::isFD() const {
  return mFD;
}

MBOOL P2Util::SimpleOut::isMDPOutput() const {
  ID_IMG outID = mImg->getID();
  return (outID == OUT_YUV) || (outID == OUT_JPEG_YUV) ||
         (outID == OUT_THN_YUV);
}

}  // namespace P2
