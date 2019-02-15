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

#define LOG_TAG "MtkCam/GrallocHelper"
//
#include <camera_buffer_handle.h>
#include <cros-camera/camera_buffer_manager.h>
#include <linux/videodev2.h>
#include <map>
#include <mtkcam/utils/gralloc/IGrallocHelper.h>
#include <mtkcam/utils/std/Sync.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/Trace.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/common.h>
#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

cros::CameraBufferManager* g_cbm;

using NSCam::GrallocRequest;
using NSCam::GrallocStaticInfo;
using NSCam::IGrallocHelper;

/******************************************************************************
 *
 ******************************************************************************/
#define GRALLOC_ALIGN(value, base) (((value) + ((base)-1)) & ~((base)-1))
#define ADDRESS_OFFSET(base1, base2) (intptr_t(base1) - intptr_t(base2))
/******************************************************************************
 *
 ******************************************************************************/
namespace {
/**
 *
 */
std::string gQueryPixelFormatName(int format) {
#define _ENUM_TO_NAME_(_prefix_, _format_) \
  case _prefix_##_format_: {               \
    static std::string name(#_format_);    \
    return name;                           \
  } break

  switch (format) {
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, RGBA_8888);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, RGBX_8888);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, RGB_888);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, RGB_565);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, BGRA_8888);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, YV12);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, Y8);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, Y16);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, BLOB);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, IMPLEMENTATION_DEFINED);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, RAW_OPAQUE);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, YCbCr_420_888);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, YCbCr_422_SP);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, YCrCb_420_SP);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, YCbCr_422_I);
    _ENUM_TO_NAME_(HAL_PIXEL_FORMAT_, RAW16);
    _ENUM_TO_NAME_(NSCam::eImgFmt_, NV12);
    default:
      break;
  }

#undef _ENUM_TO_NAME_
  //
  return nullptr;
}

/**
 *
 */
std::string gQueryGrallocUsageName(int usage) {
#define _USAGE_TO_NAME_(_prefix_, _usage_) \
  case _prefix_##_usage_: {                \
    str += "|" #_usage_;                   \
  } break

  std::string str("0");
  //
  switch ((usage & GRALLOC_USAGE_SW_READ_MASK)) {
    _USAGE_TO_NAME_(GRALLOC_USAGE_, SW_READ_RARELY);
    _USAGE_TO_NAME_(GRALLOC_USAGE_, SW_READ_OFTEN);
    default:
      break;
  }
  //
  switch ((usage & GRALLOC_USAGE_SW_WRITE_MASK)) {
    _USAGE_TO_NAME_(GRALLOC_USAGE_, SW_WRITE_RARELY);
    _USAGE_TO_NAME_(GRALLOC_USAGE_, SW_WRITE_OFTEN);
    default:
      break;
  }
  //
  switch ((usage & GRALLOC_USAGE_HW_CAMERA_MASK)) {
    _USAGE_TO_NAME_(GRALLOC_USAGE_, HW_CAMERA_WRITE);
    _USAGE_TO_NAME_(GRALLOC_USAGE_, HW_CAMERA_READ);
    _USAGE_TO_NAME_(GRALLOC_USAGE_, HW_CAMERA_ZSL);
    default:
      break;
  }
  //
  if ((usage & GRALLOC_USAGE_HW_MASK) != 0) {
#define _USAGE_TO_NAME_OR_(_prefix_, _usage_) \
  if ((usage & _prefix_##_usage_)) {          \
    str += "|" #_usage_;                      \
  }

    _USAGE_TO_NAME_OR_(GRALLOC_USAGE_, HW_TEXTURE);
    _USAGE_TO_NAME_OR_(GRALLOC_USAGE_, HW_RENDER);
    _USAGE_TO_NAME_OR_(GRALLOC_USAGE_, HW_2D);
    _USAGE_TO_NAME_OR_(GRALLOC_USAGE_, HW_COMPOSER);
    _USAGE_TO_NAME_OR_(GRALLOC_USAGE_, HW_FB);
    _USAGE_TO_NAME_OR_(GRALLOC_USAGE_, HW_VIDEO_ENCODER);

#undef _USAGE_TO_NAME_OR_
  }
  //
#undef _USAGE_TO_NAME_
  return str;
}

/**
 *
 */
std::string gQueryDataspaceName(int32_t dataspace) {
  std::string str("_UNKNOWN_");
  return str;
}

};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
namespace {
/**
 *
 */
struct MyStaticInfo : public GrallocStaticInfo {
  int usage;
  size_t allocSize;
  //
  MyStaticInfo() {
    format = 0;
    usage = 0;
    widthInPixels = 0;
    heightInPixels = 0;
    allocSize = 0;
  }
};
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
namespace {
/**
 *
 */
MERROR
queryStaticInfo(buffer_handle_t const bh, MyStaticInfo* info) {
  int v4l2Fmt = g_cbm->GetV4L2PixelFormat(bh);
  int zsl = info->usage & GRALLOC_USAGE_HW_CAMERA_ZSL;
  switch (v4l2Fmt) {
    case V4L2_PIX_FMT_YUYV:
      if (zsl == GRALLOC_USAGE_HW_CAMERA_ZSL) {
        info->format = HAL_PIXEL_FORMAT_RAW_OPAQUE;
      } else {
        info->format = HAL_PIXEL_FORMAT_YCbCr_422_I;
      }
      break;
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV21M:
      info->format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
      break;
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      info->format = NSCam::eImgFmt_NV12;
      break;
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YVU420M:
      info->format = HAL_PIXEL_FORMAT_YV12;
      break;
    case V4L2_PIX_FMT_JPEG:
      info->format = HAL_PIXEL_FORMAT_BLOB;
      break;
  }

  MY_LOGI(" format: %#x(%s)", info->format,
          gQueryPixelFormatName(info->format).c_str());

  switch (info->format) {
    case HAL_PIXEL_FORMAT_Y8:
    case HAL_PIXEL_FORMAT_BLOB: {
      info->planes.resize(1);
      {
        MyStaticInfo::Plane& plane = info->planes.at(0);

        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 0);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 0);
        info->allocSize += plane.sizeInBytes;
      }
    } break;
    case HAL_PIXEL_FORMAT_YCbCr_422_I: {
      info->planes.resize(1);
      {
        MyStaticInfo::Plane& plane = info->planes.at(0);
        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 0);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 0);
        info->allocSize += plane.sizeInBytes;
      }
    } break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP: {
      info->planes.resize(2);
      {
        // Y
        MyStaticInfo::Plane& plane = info->planes.at(0);
        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 0);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 0);
        info->allocSize += plane.sizeInBytes;
      }
      {
        // CrCb
        MyStaticInfo::Plane& plane = info->planes.at(1);
        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 1);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 1);
        info->allocSize += plane.sizeInBytes;
      }
    } break;
    case HAL_PIXEL_FORMAT_YV12: {
      info->planes.resize(3);
      {
        // Y
        MyStaticInfo::Plane& plane = info->planes.at(0);
        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 0);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 0);
        info->allocSize += plane.sizeInBytes;
      }
      {
        // Cr
        MyStaticInfo::Plane& plane = info->planes.at(1);
        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 1);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 1);
        info->allocSize += plane.sizeInBytes;
      }
      {
        // Cb
        MyStaticInfo::Plane& plane = info->planes.at(2);
        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 2);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 2);
        info->allocSize += plane.sizeInBytes;
      }
    } break;
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED: {
      MY_LOGD("only in zsl mode: %#x(%s)", info->format,
              gQueryPixelFormatName(info->format).c_str());
      info->planes.resize(1);
      {
        MyStaticInfo::Plane& plane = info->planes.at(0);
        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 0);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 0);
        info->allocSize += plane.sizeInBytes;
      }
    } break;
    case HAL_PIXEL_FORMAT_RAW_OPAQUE: {
      MY_LOGD("only in zsl mode: %#x(%s)", info->format,
              gQueryPixelFormatName(info->format).c_str());
      info->planes.resize(1);
      {
        MyStaticInfo::Plane& plane = info->planes.at(0);
        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 0);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 0);
        info->allocSize += plane.sizeInBytes;
      }
    } break;
    case NSCam::eImgFmt_NV12: {
      info->planes.resize(2);
      {
        // Y
        MyStaticInfo::Plane& plane = info->planes.at(0);
        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 0);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 0);
        info->allocSize += plane.sizeInBytes;
      }
      {
        // CrCb
        MyStaticInfo::Plane& plane = info->planes.at(1);
        plane.rowStrideInBytes = g_cbm->GetPlaneStride(bh, 1);
        plane.sizeInBytes = g_cbm->GetPlaneSize(bh, 1);
        info->allocSize += plane.sizeInBytes;
      }
    } break;
    default:
      MY_LOGE("Not support format: %#x(%s)", info->format,
              gQueryPixelFormatName(info->format).c_str());
      return NAME_NOT_FOUND;
  }
  //
  return OK;
}
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
namespace {
class GrallocHelperImp : public IGrallocHelper {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  /**
   *
   */
  struct MyRequest : public GrallocRequest {
    explicit MyRequest(GrallocRequest const& rhs = GrallocRequest())
        : GrallocRequest(rhs) {}

    bool operator==(MyRequest const& rhs) const {
      return usage == rhs.usage && format == rhs.format &&
             widthInPixels == rhs.widthInPixels &&
             heightInPixels == rhs.heightInPixels;
    }

    bool operator<(MyRequest const& rhs) const {
      /*
          true : if lhs < rhs
          false: if lhs >= rhs
       */
      if (format != rhs.format) {
        return format < rhs.format;
      }
      if (usage != rhs.usage) {
        return usage < rhs.usage;
      }
      if (widthInPixels != rhs.widthInPixels) {
        return widthInPixels < rhs.widthInPixels;
      }
      if (heightInPixels != rhs.heightInPixels) {
        return heightInPixels < rhs.heightInPixels;
      }
      //
      //  here: lhs >= rhs
      return false;
    }
  };
  typedef std::map<MyRequest, MyStaticInfo> Map_t;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                Data Members.
  mutable std::mutex mLock;
  mutable Map_t mMap;
  bool mInitialized;

 protected:  ////                Operations.
  MERROR initialize();

  static MERROR determine(GrallocRequest const& src, MyRequest* dst);
  static MERROR determine(MyRequest const& src, MyStaticInfo* dst);

  ssize_t addToMapLocked(MyRequest const& k, MyStaticInfo const& v) const;

  void dump(MyRequest const& k, MyStaticInfo const& v) const;

 public:  ////                Operations.
  GrallocHelperImp();
  ~GrallocHelperImp();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IGrallocHelper Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Operations.
  virtual int query(struct GrallocRequest const* pRequest,
                    struct GrallocStaticInfo* pStaticInfo) const;

  virtual int query(buffer_handle_t bufHandle,
                    int usage,
                    struct GrallocStaticInfo* pStaticInfo) const;

  virtual std::string queryPixelFormatName(int format) const {
    return gQueryPixelFormatName(format);
  }

  virtual std::string queryGrallocUsageName(int usage) const {
    return gQueryGrallocUsageName(usage);
  }

  virtual std::string queryDataspaceName(int32_t dataspace) const {
    return gQueryDataspaceName(dataspace);
  }

  virtual void dumpToLog() const;
};

GrallocHelperImp* getSingleton() {
  static GrallocHelperImp* tmp = new GrallocHelperImp();
  return tmp;
}

};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
IGrallocHelper* IGrallocHelper::singleton() {
  return getSingleton();
}

/******************************************************************************
 *
 ******************************************************************************/
GrallocHelperImp::GrallocHelperImp() : mInitialized(false) {
  FUNC_START;
  //
  MERROR err = initialize();
  MY_LOGE_IF(err, "%s initialize fail", __FUNCTION__);
  FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/
GrallocHelperImp::~GrallocHelperImp() {
  FUNC_START;
  g_cbm = nullptr;
  FUNC_END;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
GrallocHelperImp::initialize() {
  FUNC_START;

  g_cbm = cros::CameraBufferManager::GetInstance();
  if (g_cbm == nullptr) {
    return NO_INIT;
  }
  //
  mInitialized = true;
  FUNC_END;
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
int GrallocHelperImp::query(struct GrallocRequest const* pRequest,
                            struct GrallocStaticInfo* pStaticInfo) const {
  MERROR status = OK;
  //
  if (!pRequest) {
    MY_LOGE("NULL pRequest");
    return BAD_VALUE;
  }
  //
  if (!pStaticInfo) {
    MY_LOGE("NULL pStaticInfo");
    return BAD_VALUE;
  }
  //
  MY_LOGD("Grallc Request: u(0x%x) f(0x%x) w(%d) h(%d)", pRequest->usage,
          pRequest->format, pRequest->widthInPixels, pRequest->heightInPixels);
  MyRequest request;
  determine(*pRequest, &request);
  //
  std::lock_guard<std::mutex> _l(mLock);
  Map_t::iterator it;
  it = mMap.find(request);
  //
  if (it == mMap.end()) {
    MyStaticInfo staticInfo;
    status = determine(request, &staticInfo);
    if (OK != status) {
      return status;
    }

    addToMapLocked(request, staticInfo);
    *pStaticInfo = staticInfo;
    switch (request.format) {
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      case HAL_PIXEL_FORMAT_YCbCr_420_888:
      case HAL_PIXEL_FORMAT_RAW_OPAQUE: {
        request.format = staticInfo.format;
        request.usage = 0;
        auto iter = mMap.find(request);
        if (iter == mMap.end()) {
          addToMapLocked(request, staticInfo);
        }
      } break;
      default:
        break;
    }
  } else {
    *pStaticInfo = it->second;
  }
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
int GrallocHelperImp::query(buffer_handle_t bufHandle,
                            int usage,
                            struct GrallocStaticInfo* pStaticInfo) const {
  MERROR status = OK;
  //
  if (!bufHandle) {
    MY_LOGE("NULL buffer_handle_t");
    return BAD_VALUE;
  }
  auto h = reinterpret_cast<const struct camera_buffer_handle*>(bufHandle);
  //
  MyRequest request;
  request.usage = usage;
  request.format = h->hal_pixel_format;
  request.widthInPixels = h->width;
  request.heightInPixels = h->height;
  //
  std::lock_guard<std::mutex> _l(mLock);
  Map_t::iterator it;
  it = mMap.find(request);
  if (it != mMap.end()) {
    *pStaticInfo = it->second;
  } else {
    MY_LOGD("Not found: %dx%d %s", request.widthInPixels,
            request.heightInPixels,
            gQueryPixelFormatName(request.format).c_str());
    MyStaticInfo staticInfo;
    staticInfo.usage = usage;
    staticInfo.widthInPixels = h->width;
    staticInfo.heightInPixels = h->height;

    status = queryStaticInfo(bufHandle, &staticInfo);
    if (OK != status) {
      return status;
    }
    //
    //  Add it to map if not existing.
    MyRequest request;
    request.usage = staticInfo.usage;
    request.format = staticInfo.format;
    request.widthInPixels = staticInfo.widthInPixels;
    request.heightInPixels = staticInfo.heightInPixels;

    //
    Map_t::iterator it;
    it = mMap.find(request);
    if (it == mMap.end()) {
      addToMapLocked(request, staticInfo);
    }
    //
    if (NULL != pStaticInfo) {
      *pStaticInfo = staticInfo;
    }
  }
  //
  return status;
}

/***************************************************************************
***

 *
 ****************************************************************************
**/
MERROR
GrallocHelperImp::determine(GrallocRequest const& src, MyRequest* dst) {
  dst->usage = src.usage;
  dst->format = src.format;
  dst->widthInPixels = src.widthInPixels;
  dst->heightInPixels = src.heightInPixels;

  //
  switch (src.format) {
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case HAL_PIXEL_FORMAT_RAW_OPAQUE:
      dst->usage = src.usage;
      break;
    default:
      dst->usage = 0;
      break;
  }

  //
  return OK;
}

/***************************************************************************
***

 *
 ****************************************************************************
**/
MERROR
GrallocHelperImp::determine(MyRequest const& src, MyStaticInfo* dst) {
  //
  // already protected by mLock.
  // allocate null buffer for query static info.
  // free buffer handle before end of this function.
  uint32_t outStride = 0;
  buffer_handle_t handle;
  int err = g_cbm->Allocate(src.widthInPixels, src.heightInPixels, src.format,
                            src.usage, cros::GRALLOC, &handle, &outStride);
  if (err != OK || handle == nullptr) {
    MY_LOGE("Bad allcation handle:%p %dx%d format:%#x(%s) usage:%#x(%s)",
            handle, src.widthInPixels, src.heightInPixels, src.format,
            gQueryPixelFormatName(src.format).c_str(), src.usage,
            gQueryGrallocUsageName(src.usage).c_str());
    return NO_MEMORY;
  }
  dst->usage = src.usage;
  dst->widthInPixels = src.widthInPixels;
  dst->heightInPixels = src.heightInPixels;
  err = queryStaticInfo(handle, dst);
  //
  if (handle) {
    g_cbm->Free(handle);
  }

  return err;
}

/******************************************************************************
 *
 ******************************************************************************/
ssize_t GrallocHelperImp::addToMapLocked(MyRequest const& k,
                                         MyStaticInfo const& v) const {
  dump(k, v);
  mMap.insert(std::pair<MyRequest, MyStaticInfo>(k, v));
  return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
void GrallocHelperImp::dumpToLog() const {
  MY_LOGD("+");
  std::lock_guard<std::mutex> _l(mLock);
  for (auto& i : mMap) {
    dump(i.first, i.second);
  }
  MY_LOGD("-");
}

/******************************************************************************
 *
 ******************************************************************************/
void GrallocHelperImp::dump(MyRequest const& k, MyStaticInfo const& v) const {
  MY_LOGD(
      "************************************************************************"
      "*****");
  MY_LOGD("%dx%d usage:0x%08x(%s) format:0x%08x(%s) -->", k.widthInPixels,
          k.heightInPixels, k.usage, queryGrallocUsageName(k.usage).c_str(),
          k.format, queryPixelFormatName(k.format).c_str());
  MY_LOGD("%dx%d usage:0x%08x(%s) format:0x%08x(%s)", v.widthInPixels,
          v.heightInPixels, v.usage, queryGrallocUsageName(v.usage).c_str(),
          v.format, queryPixelFormatName(v.format).c_str());
  for (size_t i = 0; i < v.planes.size(); i++) {
    MY_LOGD("  [%zu] sizeInBytes:%zu rowStrideInBytes:%zu", i,
            v.planes[i].sizeInBytes, v.planes[i].rowStrideInBytes);
  }
}
