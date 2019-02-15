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

#include <mtkcam/feature/featurePipe/SFPIO.h>
#include <mtkcam/feature/utils/p2/P2Util.h>
#include <string>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

/*******************************************
SFPOutput
*******************************************/

SFPOutput::SFPOutput(IImageBuffer* buffer,
                     MUINT32 transform,
                     OutTargetType targetType)
    : Feature::P2Util::P2IO(buffer, transform, NSIoPipe::EPortCapbility_None),
      mTargetType(targetType),
      mDMAConstrainFlag(Feature::P2Util::DMACONSTRAIN_2BYTEALIGN |
                        Feature::P2Util::DMACONSTRAIN_NOSUBPIXEL)

{}

SFPOutput::SFPOutput(const NSCam::NSIoPipe::Output& qOut,
                     const NSCam::NSIoPipe::MCrpRsInfo& crop,
                     MVOID* pqPtr,
                     MVOID* dppqPtr,
                     OutTargetType targetType)
    : Feature::P2Util::P2IO(
          qOut.mBuffer, qOut.mTransform, qOut.mPortID.capbility),
      mTargetType(targetType),
      mCropRect(MRectF(crop.mCropRect.p_integral, crop.mCropRect.s)),
      mCropDstSize(crop.mResizeDst),
      mpPqParam(pqPtr),
      mpDpPqParam(dppqPtr),
      mDMAConstrainFlag(Feature::P2Util::DMACONSTRAIN_2BYTEALIGN |
                        Feature::P2Util::DMACONSTRAIN_NOSUBPIXEL) {}

SFPOutput::SFPOutput()
    : mDMAConstrainFlag(Feature::P2Util::DMACONSTRAIN_2BYTEALIGN |
                        Feature::P2Util::DMACONSTRAIN_NOSUBPIXEL) {}

const char* SFPOutput::typeToChar(const OutTargetType& type) {
  switch (type) {
    case OUT_TARGET_UNKNOWN:
      return "unknown";
    case OUT_TARGET_DISPLAY:
      return "disp";
    case OUT_TARGET_RECORD:
      return "rec";
    case OUT_TARGET_FD:
      return "fd";
    case OUT_TARGET_PHYSICAL:
      return "phy";
    default:
      return "invalid";
  }
}

MVOID SFPOutput::appendDumpInfo(std::string* str) const {
  MSize size = (mBuffer != NULL) ? mBuffer->getImgSize() : MSize(0, 0);
  base::StringAppendF(str,
                      "[buf(%p)(%dx%d),tran(%u),cap(%d), tar(%s), "
                      "crop(%f,%f,%fx%f), pq(%p/%p) flag(0x%x)]",
                      mBuffer, size.w, size.h, mTransform, mCapability,
                      typeToChar(mTargetType), mCropRect.p.x, mCropRect.p.y,
                      mCropRect.s.w, mCropRect.s.h, mpPqParam, mpDpPqParam,
                      mDMAConstrainFlag);
}

MBOOL SFPOutput::isCropValid() const {
  return (mCropRect.s.w > 0.0f && mCropRect.s.h > 0.0f &&
          mCropDstSize.w > 0.0f && mCropDstSize.h > 0.0f);
}

MVOID SFPOutput::convertTo(NSCam::NSIoPipe::Output* qOut) const {
  qOut->mBuffer = mBuffer;
  qOut->mTransform = mTransform;
  qOut->mPortID.capbility = mCapability;
}

MVOID SFPOutput::convertTo(NSCam::NSIoPipe::MCropRect* cropRect) const {
  *cropRect = Feature::P2Util::getCropRect(mCropRect);

  if ((mDMAConstrainFlag & Feature::P2Util::DMACONSTRAIN_NOSUBPIXEL) ||
      (mDMAConstrainFlag & Feature::P2Util::DMACONSTRAIN_2BYTEALIGN)) {
    cropRect->p_fractional.x = 0;
    cropRect->p_fractional.y = 0;
    cropRect->w_fractional = 0;
    cropRect->h_fractional = 0;
    if (mDMAConstrainFlag & Feature::P2Util::DMACONSTRAIN_2BYTEALIGN) {
      cropRect->p_integral.x &= (~1);
      cropRect->p_integral.y &= (~1);
    }
  }
}

MVOID SFPOutput::convertTo(NSCam::NSIoPipe::MCrpRsInfo* cropInfo) const {
  convertTo(&(cropInfo->mCropRect));
  cropInfo->mResizeDst = mCropDstSize;
}

/*******************************************
SFPSensorInput
*******************************************/
MVOID SFPSensorInput::appendDumpInfo(std::string* str, MUINT32 sID) const {
  base::StringAppendF(str,
                      "[sID(%d)--IMG(%p),RRZ(%p),LCS(%p),pRSS(%p),cRSS(%p),"
                      "HalI(%p),AppI(%p),AppDI(%p),AppOver(%p)]",
                      sID, mIMGO, mRRZO, mLCSO, mPrvRSSO, mCurRSSO, mHalIn,
                      mAppIn, mAppDynamicIn, mAppInOverride);
}

/*******************************************
SFPSensorTuning
*******************************************/

MBOOL SFPSensorTuning::isRRZOin() const {
  return (mFlag & FLAG_RRZO_IN);
}

MBOOL SFPSensorTuning::isIMGOin() const {
  return (mFlag & FLAG_IMGO_IN);
}
MBOOL SFPSensorTuning::isLCSOin() const {
  return (mFlag & FLAG_LCSO_IN);
}
MBOOL SFPSensorTuning::isDisable3DNR() const {
  return (mFlag & FLAG_FORCE_DISABLE_3DNR);
}

MVOID SFPSensorTuning::addFlag(Flag flag) {
  mFlag |= flag;
}

MBOOL SFPSensorTuning::isValid() const {
  return mFlag != 0;
}

MVOID SFPSensorTuning::appendDumpInfo(std::string* str) const {
  base::StringAppendF(str, "[flag(%d)]", mFlag);
}

/*******************************************
SFPIOMap
*******************************************/

const char* SFPIOMap::pathToChar(const PathType& type) {
  switch (type) {
    case PATH_GENERAL:
      return "GEN";
    case PATH_PHYSICAL:
      return "PHY";
    case PATH_LARGE:
      return "LARGE";
    case PATH_UNKNOWN:
    default:
      return "invalid";
  }
}

MVOID SFPIOMap::addInputTuning(MUINT32 sensorID, const SFPSensorTuning& input) {
  mInputMap[sensorID] = input;
}

MBOOL SFPIOMap::hasTuning(MUINT32 sensorID) const {
  return mInputMap.count(sensorID) > 0;
}

const SFPSensorTuning& SFPIOMap::getTuning(MUINT32 sensorID) const {
  if (mInputMap.count(sensorID) > 0) {
    return mInputMap.at(sensorID);
  } else {
    return mDummy;
  }
}

MVOID SFPIOMap::addOutput(const SFPOutput& out) {
  mOutList.push_back(out);
}

MVOID SFPIOMap::getAllOutput(std::vector<SFPOutput>* outList) const {
  *outList = mOutList;
}

MBOOL SFPIOMap::isValid() const {
  return (mPathType != PATH_UNKNOWN) && (!mOutList.empty());
}
MBOOL SFPIOMap::isGenPath() const {
  return (mPathType == PATH_GENERAL);
}

const char* SFPIOMap::pathName() const {
  return pathToChar(mPathType);
}

MVOID SFPIOMap::appendDumpInfo(std::string* str) const {
  base::StringAppendF(str, "{path(%s),halO(%p),appO(%p),Outs--",
                      pathToChar(mPathType), mHalOut, mAppOut);
  for (auto& out : mOutList) {
    out.appendDumpInfo(str);
  }

  for (auto& in : mInputMap) {
    base::StringAppendF(str, "Tuning--id(%u)--", in.first);
    in.second.appendDumpInfo(str);
  }
  str->append("}");
}

MUINT32 SFPIOMap::getFirstSensorID() {
  if (mFirstID != INVALID_SENSOR_ID) {
    return mFirstID;
  }
  for (auto& it : mInputMap) {
    mFirstID = it.first;
  }
  return mFirstID;
}

MVOID SFPIOMap::getAllSensorIDs(std::vector<MUINT32>* ids) const {
  for (auto& it : mInputMap) {
    ids->push_back(it.first);
  }
}

MBOOL SFPIOMap::isSameTuning(const SFPIOMap& map1,
                             const SFPIOMap& map2,
                             MUINT32 sensorID) {
  if (!map1.isValid() || !map2.isValid()) {
    return MFALSE;
  }
  const SFPSensorTuning& tun1 = map1.getTuning(sensorID);
  const SFPSensorTuning& tun2 = map2.getTuning(sensorID);
  if (!tun1.isValid() || !tun2.isValid()) {
    return MFALSE;
  }
  return (tun1.mFlag == tun2.mFlag);
}

/*******************************************
SFPIOManager
*******************************************/
MBOOL SFPIOManager::addInput(MUINT32 sensorID, const SFPSensorInput& input) {
  mSensorInputs[sensorID] = input;
  return MTRUE;
}

MBOOL SFPIOManager::addGeneral(const SFPIOMap& sfpio) {
  mGenerals.push_back(sfpio);
  return (mGenerals.size() <= 1);  // Currently not allow more than 1 general
}

MBOOL SFPIOManager::addPhysical(MUINT32 sensorID, const SFPIOMap& sfpio) {
  if (mPhysicals.count(sensorID) > 0 || sensorID == INVALID_SENSOR_ID) {
    return MFALSE;
  }
  mPhysicals[sensorID] = sfpio;
  return MTRUE;
}

MBOOL SFPIOManager::addLarge(MUINT32 sensorID, const SFPIOMap& sfpio) {
  if (mLarges.count(sensorID) > 0 || sensorID == INVALID_SENSOR_ID) {
    return MFALSE;
  }
  mLarges[sensorID] = sfpio;
  return MTRUE;
}

const SFPSensorInput& SFPIOManager::getInput(MUINT32 sensorID) {
  if (mSensorInputs.count(sensorID) > 0) {
    return mSensorInputs.at(sensorID);
  } else {
    return mDummyInput;
  }
}

const std::vector<SFPIOMap>& SFPIOManager::getGeneralIOs() const {
  return mGenerals;
}

MUINT32 SFPIOManager::countAll() const {
  return mGenerals.size() + mPhysicals.size() + mLarges.size();
}

MUINT32 SFPIOManager::countNonLarge() const {
  return mGenerals.size() + mPhysicals.size();
}

MUINT32 SFPIOManager::countLarge() const {
  return mLarges.size();
}

MUINT32 SFPIOManager::countGeneral() const {
  return mGenerals.size();
}

const SFPIOMap& SFPIOManager::getFirstGeneralIO() {
  if (mGenerals.size() > 0) {
    return mGenerals.front();
  } else {
    return mDummy;
  }
}

MVOID SFPIOManager::appendDumpInfo(std::string* str) const {
  for (auto& it : mSensorInputs) {
    it.second.appendDumpInfo(str, it.first);
  }
  for (auto&& io : mGenerals) {
    io.appendDumpInfo(str);
  }
  for (auto&& it : mPhysicals) {
    it.second.appendDumpInfo(str);
  }
  for (auto&& it : mLarges) {
    it.second.appendDumpInfo(str);
  }
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
