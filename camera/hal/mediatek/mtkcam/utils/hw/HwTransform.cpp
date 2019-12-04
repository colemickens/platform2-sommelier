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

#define LOG_TAG "MtkCam/HwTransHelper"
//
#include <cmath>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/hw/HwTransform.h>
#include <mutex>
#include <unordered_map>
#include <utility>

using NSCam::IHalSensor;
using NSCam::IHalSensorList;
using NSCam::SensorCropWinInfo;
using NSCam::SensorStaticInfo;
using NSCamHW::HwMatrix;
using NSCamHW::HwTransHelper;
using NSCamHW::simplifiedMatrix;

#define ACTIVEARRAY_MODE NSCam::SENSOR_SCENARIO_ID_NORMAL_CAPTURE

/*******************************************************************************
 *
 ******************************************************************************/
MVOID
simplifiedMatrix::dump(const char* const str) const {
  MY_LOGD("%s: (%f, %f, %f, %f)", str, c_00, c_02, c_11, c_12);
}

/*******************************************************************************
 *
 ******************************************************************************/
namespace {
class SensorInfo {
 public:
  explicit SensorInfo(MINT32 const openId) : mOpenId(openId) {}
  MBOOL getMatrix(MUINT32 const sensorMode,
                  HwMatrix* mat,  // mat: transform from active to sensor mode
                  HwMatrix* mat_inv,
                  MBOOL forceAspRatioAlign = MFALSE);
  // Note: getMatrix(...) should be called before this getCropInfo(...)
  MBOOL getCropInfo(MUINT32 const sensorMode,
                    SensorCropWinInfo* cropInfo_mode,
                    SensorCropWinInfo* cropInfo_active);

 private:
  std::mutex mLock;
  MINT32 const mOpenId;

  struct cropItem {
    SensorCropWinInfo sensor_crop_info;
    HwMatrix trans;
    HwMatrix inv_trans;
    HwMatrix trans_ratio_align;
    HwMatrix inv_trans_ratio_align;
  };
  // key: sensor mode
  // value: cropItem
  std::unordered_map<MUINT32, cropItem> mvCropInfos;
};
}  // namespace

/*******************************************************************************
 *
 ******************************************************************************/
MBOOL
SensorInfo::getMatrix(MUINT32 const sensorMode,
                      HwMatrix* mat,
                      HwMatrix* mat_inv,
                      MBOOL forceAspRatioAlign) {
  struct updater {
    IHalSensorList* pSensorList;
    IHalSensor* pSensorHalObj;
    //
    updater() : pSensorList(NULL), pSensorHalObj(NULL) {}
    ~updater() {
      if (pSensorHalObj) {
        pSensorHalObj->destroyInstance(LOG_TAG);
      }
    }
    //
    MBOOL
    operator()(MINT32 const openId,
               MUINT32 const sensorMode,
               SensorCropWinInfo* cropInfo) {
      pSensorList = GET_HalSensorList();
      if (!pSensorList ||
          !(pSensorHalObj = pSensorList->createSensor(LOG_TAG, openId))) {
        MY_LOGE("fail pSensorList(%p), pSensorHal(%p)", pSensorList,
                pSensorHalObj);
        return MFALSE;
      }
      //
      MINT32 err = pSensorHalObj->sendCommand(
          pSensorList->querySensorDevIdx(openId),
          NSCam::SENSOR_CMD_GET_SENSOR_CROP_WIN_INFO, (MUINTPTR)&sensorMode,
          sizeof(MUINT32), (MUINTPTR)cropInfo, sizeof(SensorCropWinInfo), 0,
          sizeof(MUINT32));
      //
      if (err != 0 || cropInfo->full_w == 0 || cropInfo->full_h == 0 ||
          cropInfo->w0_size == 0 || cropInfo->h0_size == 0) {
        MY_LOGW(
            "cannot get proper sensor %d crop win info of mode (%d): use "
            "default",
            openId, sensorMode);
        //
        SensorStaticInfo staticInfo;
        memset(&staticInfo, 0, sizeof(SensorStaticInfo));
        //
        pSensorList->querySensorStaticInfo(
            pSensorList->querySensorDevIdx(openId), &staticInfo);
        //
        MSize tgsize;
        switch (sensorMode) {
#define sensor_mode_case(_mode_, _key_, _size_) \
  case _mode_:                                  \
    _size_.w = staticInfo._key_##Width;         \
    _size_.h = staticInfo._key_##Height;        \
    break;
          sensor_mode_case(NSCam::SENSOR_SCENARIO_ID_NORMAL_PREVIEW, preview,
                           tgsize);
          sensor_mode_case(NSCam::SENSOR_SCENARIO_ID_NORMAL_CAPTURE, capture,
                           tgsize);
          sensor_mode_case(NSCam::SENSOR_SCENARIO_ID_NORMAL_VIDEO, video,
                           tgsize);
          sensor_mode_case(NSCam::SENSOR_SCENARIO_ID_SLIM_VIDEO1, video1,
                           tgsize);
          sensor_mode_case(NSCam::SENSOR_SCENARIO_ID_SLIM_VIDEO2, video2,
                           tgsize);
          sensor_mode_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM1, SensorCustom1,
                           tgsize);
          sensor_mode_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM2, SensorCustom2,
                           tgsize);
          sensor_mode_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM3, SensorCustom3,
                           tgsize);
          sensor_mode_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM4, SensorCustom4,
                           tgsize);
          sensor_mode_case(NSCam::SENSOR_SCENARIO_ID_CUSTOM5, SensorCustom5,
                           tgsize);
#undef sensor_mode_case
          default:
            MY_LOGE("not support sensor scenario(0x%x)", sensorMode);
            return MFALSE;
        }
        //
        cropInfo->x0_offset = 0;
        cropInfo->y0_offset = 0;
        cropInfo->w0_size = tgsize.w;
        cropInfo->h0_size = tgsize.h;
        //
        cropInfo->scale_w = tgsize.w;
        cropInfo->scale_h = tgsize.h;
        //
        cropInfo->x1_offset = 0;
        cropInfo->y1_offset = 0;
        cropInfo->w1_size = tgsize.w;
        cropInfo->h1_size = tgsize.w;
        //
        cropInfo->x2_tg_offset = 0;
        cropInfo->y2_tg_offset = 0;
        cropInfo->w2_tg_size = tgsize.w;
        cropInfo->h2_tg_size = tgsize.h;
      } else {
        //
        SensorStaticInfo staticInfo;
        memset(&staticInfo, 0, sizeof(SensorStaticInfo));
        //
        pSensorList->querySensorStaticInfo(
            pSensorList->querySensorDevIdx(openId), &staticInfo);
      }
      MY_LOGD("senor %d, mode %d: crop infos", openId, sensorMode);
      MY_LOGD("full %dx%d, crop0(%d,%d,%dx%d), resized(%d,%d)",
              cropInfo->full_w, cropInfo->full_h, cropInfo->x0_offset,
              cropInfo->y0_offset, cropInfo->w0_size, cropInfo->h0_size,
              cropInfo->scale_w, cropInfo->scale_h);
      MY_LOGD("crop1(%d,%d,%dx%d), tg(%d,%d,%dx%d)", cropInfo->x1_offset,
              cropInfo->y1_offset, cropInfo->w1_size, cropInfo->h1_size,
              cropInfo->x2_tg_offset, cropInfo->y2_tg_offset,
              cropInfo->w2_tg_size, cropInfo->h2_tg_size);
      return MTRUE;
    }
  };
  //
  std::lock_guard<std::mutex> _l(mLock);
  //
  std::unordered_map<MUINT32, cropItem>::iterator it;
  it = mvCropInfos.find(sensorMode);
  if (it == mvCropInfos.end()) {
    // check active array
    std::unordered_map<MUINT32, cropItem>::iterator iter;
    iter = mvCropInfos.find(ACTIVEARRAY_MODE);
    if (iter == mvCropInfos.end()) {
      cropItem itemActive;
      if (!updater()(mOpenId, ACTIVEARRAY_MODE, &itemActive.sensor_crop_info)) {
        return MFALSE;
      }
      //
      itemActive.trans = HwMatrix(1.f, 0.f, 1.f, 0.f);
      itemActive.inv_trans = HwMatrix(1.f, 0.f, 1.f, 0.f);
      //
      mvCropInfos.emplace(std::make_pair(ACTIVEARRAY_MODE, itemActive));
      iter = mvCropInfos.find(ACTIVEARRAY_MODE);
    }

    SensorCropWinInfo crop_active;
    if (iter != mvCropInfos.end()) {
      crop_active = (iter->second).sensor_crop_info;
    }
    //
    cropItem item;
    if (!updater()(mOpenId, sensorMode, &item.sensor_crop_info)) {
      return MFALSE;
    }
    //
    SensorCropWinInfo const& crop_target = item.sensor_crop_info;
    //
    struct forward_matrix {
      static HwMatrix get(SensorCropWinInfo const& info) {
        return HwMatrix(1.f, -(info.x1_offset + info.x2_tg_offset), 1.f,
                        -(info.y1_offset + info.y2_tg_offset)) *
               HwMatrix(static_cast<float>(info.scale_w) /
                            static_cast<float>(info.w0_size),
                        0.f,
                        static_cast<float>(info.scale_h) /
                            static_cast<float>(info.h0_size),
                        0.f) *
               HwMatrix(1.f, -(info.x0_offset), 1.f, -(info.y0_offset));
      }
      static HwMatrix getAlign(SensorCropWinInfo const& info) {
#define THRESHOLD 0.1
        if (std::abs((static_cast<float>(info.full_w) /
                      static_cast<float>(info.full_h)) -
                     (static_cast<float>(info.w0_size) /
                      static_cast<float>(info.h0_size))) < THRESHOLD) {
          return get(info);  // aspect ratio not change
        }
#undef THRESHOLD
        MRect crop0;
        MRect src(MPoint(info.x0_offset, info.y0_offset),
                  MSize(info.w0_size, info.h0_size));
        MSize target(info.full_w, info.full_h);
        HwTransHelper::cropAlignRatio(src, target, &crop0);
        MY_LOGD("align crop(%d,%d,%dx%d)", crop0.p.x, crop0.p.y, crop0.s.w,
                crop0.s.h);

        return HwMatrix(1.f, -(info.x1_offset + info.x2_tg_offset), 1.f,
                        -(info.y1_offset + info.y2_tg_offset)) *
               HwMatrix(static_cast<float>(info.scale_w) /
                            static_cast<float>(info.w0_size),
                        0.f,
                        static_cast<float>(info.scale_h) /
                            static_cast<float>(info.h0_size),
                        0.f) *
               HwMatrix(1.f, -(info.x0_offset - crop0.p.x), 1.f,
                        -(info.y0_offset - crop0.p.y)) *
               HwMatrix(static_cast<float>(crop0.s.w) /
                            static_cast<float>(info.full_w),
                        0.f,
                        static_cast<float>(crop0.s.h) /
                            static_cast<float>(info.full_h),
                        0.f);
      }
    };
    //
    HwMatrix active_forward = forward_matrix::get(crop_active);
    // active_forward.dump("active forward"); //debug
    //
    HwMatrix active_inv;
    if (!active_forward.getInverse(active_inv)) {
      MY_LOGE("cannot get proper inverse matrix of active");
      return MFALSE;
    }
    // active_inv.dump("active_inv"); //debug
    //
    HwMatrix target_forward = forward_matrix::get(crop_target);
    HwMatrix target_forward_align = forward_matrix::getAlign(crop_target);
    // target_forward.dump("target_forward"); //debug
    target_forward_align.dump("target_forward_align");  // debug
    //
    item.trans = target_forward * active_inv;
    item.trans_ratio_align = target_forward_align * active_inv;
    // item.trans.dump("trans"); //debug
    //
    if (!item.trans.getInverse(item.inv_trans) ||
        !item.trans_ratio_align.getInverse(item.inv_trans_ratio_align)) {
      MY_LOGE("cannot get proper inverse matrix");
    } else {
      // item.inv_trans.dump("inv_trans"); //debug
      mvCropInfos.emplace(std::make_pair(sensorMode, item));
      it = mvCropInfos.find(sensorMode);
    }
  }
  //
  if (it != mvCropInfos.end()) {
    if (mat)
      *mat = forceAspRatioAlign ? (it->second).trans_ratio_align
                                : (it->second).trans;
    if (mat_inv)
      *mat_inv = forceAspRatioAlign ? (it->second).inv_trans_ratio_align
                                    : (it->second).inv_trans;
    return MTRUE;
  }
  //
  return MFALSE;
}

/*******************************************************************************
 *
 ******************************************************************************/
MBOOL
SensorInfo::getCropInfo(MUINT32 const sensorMode,
                        SensorCropWinInfo* pcropInfo_mode,
                        SensorCropWinInfo* pcropInfo_active) {
  std::lock_guard<std::mutex> _l(mLock);
  //
  std::unordered_map<MUINT32, cropItem>::iterator it;
  it = mvCropInfos.find(sensorMode);
  if (it == mvCropInfos.end()) {
    return MFALSE;
  }
  if (pcropInfo_mode) {
    *pcropInfo_mode = (it->second).sensor_crop_info;
  }
  //
  it = mvCropInfos.find(ACTIVEARRAY_MODE);
  if (it == mvCropInfos.end()) {
    return MFALSE;
  }
  if (pcropInfo_active) {
    *pcropInfo_active = (it->second).sensor_crop_info;
  }
  //
  return MTRUE;
}

/*******************************************************************************
 *
 ******************************************************************************/
static std::mutex gLock;
static std::unordered_map<MUINT32, SensorInfo*> gvSensorInfos;

/*******************************************************************************
 *
 ******************************************************************************/
HwTransHelper::HwTransHelper(MINT32 const openId) : mOpenId(openId) {
  std::lock_guard<std::mutex> _l(gLock);

  std::unordered_map<MUINT32, SensorInfo*>::iterator it;
  it = gvSensorInfos.find(openId);
  if (it == gvSensorInfos.end()) {
    gvSensorInfos.emplace(std::make_pair(openId, new SensorInfo(openId)));
  }
}

/*******************************************************************************
 *
 ******************************************************************************/
HwTransHelper::~HwTransHelper() {}

/*******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwTransHelper::getMatrixFromActive(MUINT32 const sensorMode, HwMatrix* mat) {
  SensorInfo* pSensorInfo = NULL;
  //
  {
    std::lock_guard<std::mutex> _l(gLock);
    auto it = gvSensorInfos.find(mOpenId);
    if (it == gvSensorInfos.end()) {
      return MFALSE;
    }
    pSensorInfo = it->second;
  }
  //
  return pSensorInfo->getMatrix(sensorMode, mat, NULL);
}

/*******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwTransHelper::getMatrixToActive(MUINT32 const sensorMode, HwMatrix* mat) {
  SensorInfo* pSensorInfo = NULL;
  //
  {
    std::lock_guard<std::mutex> _l(gLock);
    auto it = gvSensorInfos.find(mOpenId);
    if (it == gvSensorInfos.end()) {
      return MFALSE;
    }
    pSensorInfo = it->second;
  }
  //
  return pSensorInfo->getMatrix(sensorMode, NULL, mat);
}

/*******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwTransHelper::getMatrixFromActiveRatioAlign(MUINT32 const sensorMode,
                                             HwMatrix* mat) {
  SensorInfo* pSensorInfo = NULL;
  //
  {
    std::lock_guard<std::mutex> _l(gLock);
    auto it = gvSensorInfos.find(mOpenId);
    if (it == gvSensorInfos.end()) {
      return MFALSE;
    }
    pSensorInfo = it->second;
  }

  //
  return pSensorInfo->getMatrix(sensorMode, mat, NULL, MTRUE);
}

/*******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwTransHelper::getMatrixToActiveRatioAlign(MUINT32 const sensorMode,
                                           HwMatrix* mat) {
  SensorInfo* pSensorInfo = NULL;
  //
  {
    std::lock_guard<std::mutex> _l(gLock);
    auto it = gvSensorInfos.find(mOpenId);
    if (it == gvSensorInfos.end()) {
      return MFALSE;
    }
    pSensorInfo = it->second;
  }

  //
  return pSensorInfo->getMatrix(sensorMode, NULL, mat, MTRUE);
}

/*******************************************************************************
 *
 ******************************************************************************/
MBOOL
HwTransHelper::calculateFovDifference(MUINT32 const sensorMode,
                                      float* fov_diff_x,
                                      float* fov_diff_y) {
  SensorInfo* pSensorInfo = NULL;
  //
  {
    std::lock_guard<std::mutex> _l(gLock);
    auto it = gvSensorInfos.find(mOpenId);
    if (it == gvSensorInfos.end()) {
      return MFALSE;
    }
    pSensorInfo = it->second;
  }

  //
  HwMatrix mat_mode2active;
  SensorCropWinInfo cropInfo_mode;
  SensorCropWinInfo cropInfo_active;
  //
  if (!pSensorInfo->getMatrix(sensorMode, NULL, &mat_mode2active)) {
    MY_LOGW("cannot get infos of mode %d", sensorMode);
    return MFALSE;
  }
  //
  if (!pSensorInfo->getCropInfo(sensorMode, &cropInfo_mode, &cropInfo_active)) {
    MY_LOGW("cannot get crop infos of mode %d", sensorMode);
    return MFALSE;
  }
  //
  MPoint topleft;
  mat_mode2active.transform(MPoint(0, 0), &topleft);
  //
  MPoint bottomright;
  mat_mode2active.transform(
      MPoint(cropInfo_mode.w2_tg_size - 1, cropInfo_mode.h2_tg_size - 1),
      &bottomright);
  //
  // calculate the fov difference
  float diff_x = std::abs(topleft.x - 0) +
                 std::abs(bottomright.x - (cropInfo_active.w2_tg_size - 1));
  float diff_y = std::abs(topleft.y - 0) +
                 std::abs(bottomright.y - (cropInfo_active.h2_tg_size - 1));
  MY_LOGD(
      "sensorMode(%d), topleft(%d,%d), btmRight(%d,%d),diff(%f, %f), "
      "cropMode(%d,%d), cropAct(%d,%d)",
      sensorMode, topleft.x, topleft.y, bottomright.x, bottomright.y, diff_x,
      diff_y, cropInfo_mode.w2_tg_size, cropInfo_mode.h2_tg_size,
      cropInfo_active.w2_tg_size, cropInfo_active.h2_tg_size);
  //
  if (fov_diff_x) {
    *fov_diff_x = diff_x / static_cast<float>(cropInfo_active.w2_tg_size);
  }
  if (fov_diff_y) {
    *fov_diff_y = diff_y / static_cast<float>(cropInfo_active.h2_tg_size);
  }
  //
  return MTRUE;
}
