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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_FORMAT_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_FORMAT_H_
//
#include <stddef.h>
#include <string>
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace Utils {
namespace Format {

bool VISIBILITY_PUBLIC checkValidFormat(int const imageFormat);

/*****************************************************************************
 * @brief Query the name of a specified format.
 *
 * @details Given a format of type EImageFormat, return its readable name.
 *
 * @note
 *
 * @param[in] imageFormat: A format of type EImageFormat.
 *
 * @return the name of image format.
 *
 ******************************************************************************/
char const* VISIBILITY_PUBLIC queryImageFormatName(int const imageFormat);

/*****************************************************************************
 * @brief Query the plane count.
 *
 * @details Given a format of type EImageFormat, return its corresponding
 * plane count.
 *
 * @note
 *
 * @param[in] imageFormat: A format of type EImageFormat.
 *
 * @return its corresponding plane count.
 *
 ******************************************************************************/
size_t VISIBILITY_PUBLIC queryPlaneCount(int const imageFormat);

/*****************************************************************************
 * @brief Query the width in pixels of a specific plane.
 *
 * @details Given a format of type EImageFormat, a plane index, and the width in
 * in pixels of the 0-th plane, return the width in pixels of the given plane.
 *
 * @note
 *
 * @param[in] imageFormat: A format of type EImageFormat.
 * @param[in] planeIndex: a specific plane index.
 * @param[in] widthInPixels: the width in pixels of the 0-th plane.
 *
 * @return the width in pixels of the given plane.
 *
 ******************************************************************************/
size_t VISIBILITY_PUBLIC queryPlaneWidthInPixels(int const imageFormat,
                                                 size_t planeIndex,
                                                 size_t widthInPixels);

/*****************************************************************************
 * @brief Query the height in pixels of a specific plane.
 *
 * @details Given a format of type EImageFormat, a plane index, and the height
 * in pixels of the 0-th plane, return the height in pixels of the given plane.
 *
 * @note
 *
 * @param[in] imageFormat: A format of type EImageFormat.
 * @param[in] planeIndex: a specific plane index.
 * @param[in] heightInPixels: the height in pixels of the 0-th plane.
 *
 * @return the height in pixels of the given plane.
 *
 ******************************************************************************/
size_t VISIBILITY_PUBLIC queryPlaneHeightInPixels(int const imageFormat,
                                                  size_t planeIndex,
                                                  size_t heightInPixels);

/*****************************************************************************
 * @brief Query the bits per pixel of a specific plane.
 *
 * @details Given a format of type EImageFormat and a plane index, return
 * the bits per pixel of the given plane.
 *
 * @note
 *
 * @param[in] imageFormat: A format of type EImageFormat.
 * @param[in] planeIndex: a specific plane index.
 *
 * @return the bits per pixel of the given plane.
 *
 ******************************************************************************/
int VISIBILITY_PUBLIC queryPlaneBitsPerPixel(int const imageFormat,
                                             size_t planeIndex);

/*****************************************************************************
 * @brief Query the bits per pixel of a specific format.
 *
 * @details Given a format of type EImageFormat, return the bits per pixel.
 *
 * @note
 *
 * @param[in] imageFormat: A format of type EImageFormat.
 *
 * @return the bits per pixel.
 *
 ******************************************************************************/
int VISIBILITY_PUBLIC queryImageBitsPerPixel(int const imageFormat);

/*****************************************************************************
 * @Dump the information of Image map.
 ******************************************************************************/
void dumpMapInformation();

/*****************************************************************************
 * @brief decide if need to check the correction of buffer info
 *
 * @details Given a format of type EImageFormat, return true/false.
 *
 * @note
 *
 * @param[in] imageFormat: A format of type EImageFormat.
 *
 * @return true if need to be checked
 ******************************************************************************/
bool VISIBILITY_PUBLIC checkValidBufferInfo(int const imageFormat);

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Format
};      // namespace Utils
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_FORMAT_H_
