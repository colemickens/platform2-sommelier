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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2UTIL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2UTIL_H_

#include <memory>

#if MTK_DP_ENABLE  // check later
#include <DpDataType.h>
#endif
#include <INormalStream.h>
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/drv/def/Dip_Notify_datatype.h>
#include <mtkcam/feature/utils/p2/P2IO.h>
#include <mtkcam/feature/utils/p2/P2Pack.h>
#include <mtkcam/utils/std/ILogger.h>

using NSImageio::NSIspio::EPortIndex;

using NSCam::NSIoPipe::EPortCapbility;
using NSCam::NSIoPipe::ExtraParam;
using NSCam::NSIoPipe::FrameParams;
using NSCam::NSIoPipe::Input;
using NSCam::NSIoPipe::MCropRect;
using NSCam::NSIoPipe::MCrpRsInfo;
using NSCam::NSIoPipe::Output;
using NSCam::NSIoPipe::PortID;
using NSCam::NSIoPipe::PQParam;
using NSCam::NSIoPipe::QParams;
using NSCam::v4l2::ENormalStreamTag;

using NS3Av3::TuningParam;

namespace NSCam {
namespace Feature {
namespace P2Util {

enum {
  CROP_IMGO = 1,
  CROP_IMG2O = 1,
  CROP_IMG3O = 1,
  CROP_WDMAO = 2,
  CROP_WROTO = 3,
};

enum DMAConstrainFlag {
  DMACONSTRAIN_NONE = 0,
  DMACONSTRAIN_2BYTEALIGN = 1 << 0,  // p2s original usage
  DMACONSTRAIN_NOSUBPIXEL = 1 << 1,  // disable MDP sub-pixel
};

class P2ObjPtr {
 public:
  _SRZ_SIZE_INFO_* srz4 = NULL;
  NSCam::NSIoPipe::PQParam* pqParam = NULL;
#if MTK_DP_ENABLE  // check later
  DpPqParam* pqWDMA = NULL;
  DpPqParam* pqWROT = NULL;
#endif
  MBOOL hasPQ = MTRUE;
};

class P2Obj {
 public:
  mutable _SRZ_SIZE_INFO_ srz4;
  mutable NSCam::NSIoPipe::PQParam pqParam;
#if MTK_DP_ENABLE  // check later
  mutable DpPqParam pqWDMA;
  mutable DpPqParam pqWROT;
#endif
  P2ObjPtr toPtrTable() const {
    P2ObjPtr ptr;
    ptr.srz4 = &srz4;
    ptr.pqParam = &pqParam;
#if MTK_DP_ENABLE
    ptr.pqWDMA = &pqWDMA;
    ptr.pqWROT = &pqWROT;
#endif
    ptr.hasPQ = MTRUE;
    return ptr;
  }
};

// Camera common function
MBOOL is4K2K(const MSize& size);
MCropRect VISIBILITY_PUBLIC getCropRect(const MRectF& rectF);

template <typename T>
MBOOL tryGet(const IMetadata& meta, MUINT32 tag, T* val) {
  MBOOL ret = MFALSE;
  IMetadata::IEntry entry = meta.entryFor(tag);
  if (!entry.isEmpty()) {
    *val = entry.itemAt(0, Type2Type<T>());
    ret = MTRUE;
  }
  return ret;
}
template <typename T>
MBOOL tryGet(const IMetadata* meta, MUINT32 tag, T* val) {
  return (meta != NULL) ? tryGet<T>(*meta, tag, val) : MFALSE;
}
template <typename T>
MBOOL trySet(IMetadata* meta, MUINT32 tag, const T& val) {
  if (meta != NULL) {
    MBOOL ret = MFALSE;
    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    ret = (meta->update(tag, entry) == OK);
    return ret;
  } else {
    return MFALSE;
  }
}
template <typename T>
T getMeta(const IMetadata& meta, MUINT32 tag, const T& val) {
  T temp;
  return tryGet(meta, tag, temp) ? temp : val;
}
template <typename T>
T getMeta(const IMetadata* meta, MUINT32 tag, const T& val) {
  T temp;
  return tryGet(meta, tag, temp) ? temp : val;
}

// Tuning function
TuningParam VISIBILITY_PUBLIC
makeTuningParam(const ILog& log,
                const P2Pack& p2Pack,
                std::shared_ptr<NS3Av3::IHal3A> hal3A,
                NS3Av3::MetaSet_T* inMetaSet,
                NS3Av3::MetaSet_T* pOutMetaSet,
                MBOOL resized,
                std::shared_ptr<IImageBuffer> tuningBuffer,
                IImageBuffer* lcso);

// Metadata function
MVOID updateExtraMeta(const P2Pack& p2Pack, IMetadata* outHal);
MVOID updateDebugExif(const P2Pack& p2Pack,
                      const IMetadata& inHal,
                      IMetadata* outHal);
MBOOL updateCropRegion(IMetadata* outHal, const MRect& rect);

// QParams util function
EPortCapbility VISIBILITY_PUBLIC toCapability(MUINT32 usage);
const char* toName(MUINT32 index);
const char* toName(EPortIndex index);
const char* toName(const PortID& port);
const char* toName(const Input& input);
const char* toName(const Output& output);
MBOOL is(const PortID& port, EPortIndex index);
MBOOL is(const Input& input, EPortIndex index);
MBOOL is(const Output& output, EPortIndex index);
MBOOL is(const PortID& port, const PortID& rhs);
MBOOL is(const Input& input, const PortID& rhs);
MBOOL is(const Output& output, const PortID& rhs);
MVOID VISIBILITY_PUBLIC printQParams(const ILog& log, const QParams& params);
MVOID printTuningParam(const ILog& log, const TuningParam& tuning);

MVOID push_in(FrameParams* frame, const PortID& portID, IImageBuffer* buffer);
MVOID push_in(FrameParams* frame, const PortID& portID, const P2IO& in);
MVOID push_out(FrameParams* frame, const PortID& portID, IImageBuffer* buffer);
MVOID push_out(FrameParams* frame,
               const PortID& portID,
               IImageBuffer* buffer,
               EPortCapbility cap,
               MINT32 transform);
MVOID VISIBILITY_PUBLIC push_out(FrameParams* frame,
                                 const PortID& portID,
                                 const P2IO& out);
MVOID push_crop(FrameParams* frame,
                MUINT32 cropID,
                const MCropRect& crop,
                const MSize& size);
MVOID VISIBILITY_PUBLIC
push_crop(FrameParams* frame,
          MUINT32 cropID,
          const MRectF& crop,
          const MSize& size,
          const MINT32 dmaConstrainFlag = (DMACONSTRAIN_2BYTEALIGN |
                                           DMACONSTRAIN_NOSUBPIXEL));

// QParams function
MVOID updateQParams(QParams* qparams,
                    const P2Pack& p2Pack,
                    const P2IOPack& io,
                    const P2ObjPtr& obj,
                    const TuningParam& tuning);
QParams VISIBILITY_PUBLIC makeQParams(const P2Pack& p2Pack,
                                      ENormalStreamTag tag,
                                      const P2IOPack& io,
                                      const P2ObjPtr& obj);
QParams VISIBILITY_PUBLIC makeQParams(const P2Pack& p2Pack,
                                      ENormalStreamTag tag,
                                      const P2IOPack& io,
                                      const P2ObjPtr& obj,
                                      const TuningParam& tuning);
MVOID updateFrameParams(FrameParams* frame,
                        const P2Pack& p2Pack,
                        const P2IOPack& io,
                        const P2ObjPtr& obj,
                        const TuningParam& tuning);
FrameParams makeFrameParams(const P2Pack& p2Pack,
                            ENormalStreamTag tag,
                            const P2IOPack& io,
                            const P2ObjPtr& obj);
FrameParams VISIBILITY_PUBLIC makeFrameParams(const P2Pack& p2Pack,
                                              ENormalStreamTag tag,
                                              const P2IOPack& io,
                                              const P2ObjPtr& obj,
                                              const TuningParam& tuning);

// DParams uitil function
#if MTK_DP_ENABLE
DpPqParam* makeDpPqParam(DpPqParam* param,
                         const P2Pack& p2Pack,
                         const Output& out);
DpPqParam* makeDpPqParam(DpPqParam* param,
                         const P2Pack& p2Pack,
                         const MUINT32 portCapabitity);
#endif

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2UTIL_H_
