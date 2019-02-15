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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_TYPEMANIP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_TYPEMANIP_H_

/******************************************************************************
 *  Type manipulation.
 ******************************************************************************/
#ifdef __cplusplus
namespace NSCam {

/**
 * @class template Int2Type.
 * @brief Int2Type<v> converts an integral constant v into a unique type.
 */
template <int v>
struct Int2Type {
  enum { value = v };
};

/**
 * @class template Type2Type.
 * @brief Type2Type<T> converts a type T into a unique type.
 */
template <typename T>
struct Type2Type {
  typedef T OriginalType;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // __cplusplus
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_TYPEMANIP_H_
