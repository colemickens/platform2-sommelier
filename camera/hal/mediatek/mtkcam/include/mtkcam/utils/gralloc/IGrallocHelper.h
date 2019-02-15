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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_GRALLOC_IGRALLOCHELPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_GRALLOC_IGRALLOCHELPER_H_
//
#include <string>
#include <vector>
#include <system/graphics.h>
#include <hardware/gralloc.h>
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/**
 *
 */
struct GrallocRequest {
  /**
   * The gralloc usage.
   */
  int usage;

  /**
   * The image format to request.
   */
  int format;

  /**
   * The image width in pixels. For formats where some color channels are
   * subsampled, this is the width of the largest-resolution plane.
   */
  int widthInPixels;

  /**
   * The image height in pixels. For formats where some color channels are
   * subsampled, this is the height of the largest-resolution plane.
   */
  int heightInPixels;
};

/**
 *
 */
struct GrallocStaticInfo {
  struct Plane {
    /**
     * The size for this color plane, in bytes.
     */
    size_t sizeInBytes;

    /**
     * The row stride for this color plane, in bytes.
     *
     * This is the distance between the start of two consecutive rows of
     * pixels in the image. The row stride is always greater than 0.
     */
    size_t rowStrideInBytes;
  };

  /**
   * The resulting image format.
   */
  int format;

  /**
   * The image width in pixels. For formats where some color channels are
   * subsampled, this is the width of the largest-resolution plane.
   */
  int widthInPixels;

  /**
   * The image height in pixels. For formats where some color channels are
   * subsampled, this is the height of the largest-resolution plane.
   */
  int heightInPixels;

  /**
   * A vector of planes.
   */
  std::vector<Plane> planes;
};

/**
 *
 */
struct GrallocDynamicInfo {
  /**
   * A vector of ion Fds.
   */
  std::vector<int> ionFds;
};

/**
 *
 */
class VISIBILITY_PUBLIC IGrallocHelper {
 public:  ////                Instantiation.
  virtual ~IGrallocHelper() {}

  /**
   * Get a singleton instance.
   */
  static IGrallocHelper* singleton();

 public:  ////                Interfaces.
  /**
   * Given a gralloc request, return its information.
   */
  virtual int query(struct GrallocRequest const* pRequest,
                    struct GrallocStaticInfo* pStaticInfo) const = 0;

  /**
   * Given a gralloc buffer handle, return its information.
   */
  virtual int query(buffer_handle_t bufHandle,
                    int usage,
                    struct GrallocStaticInfo* pStaticInfo) const = 0;

  /**
   * Given a HAL_PIXEL_FORMAT_xxx format, return a std::string name.
   */
  virtual std::string queryPixelFormatName(int format) const = 0;

  /**
   * Given a combination of usage, return a std::string name.
   */
  virtual std::string queryGrallocUsageName(int usage) const = 0;

  /**
   * Given a dataspace, return a std::string name.
   */
  virtual std::string queryDataspaceName(int32_t dataspace) const = 0;

  /**
   * Dump to the log for debug.
   */
  virtual void dumpToLog() const = 0;
};

};  // namespace NSCam
/******************************************************************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_GRALLOC_IGRALLOCHELPER_H_
