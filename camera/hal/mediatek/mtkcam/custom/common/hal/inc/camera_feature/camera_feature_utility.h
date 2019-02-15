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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_FEATURE_CAMERA_FEATURE_UTILITY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_FEATURE_CAMERA_FEATURE_UTILITY_H_

namespace NSFeature {

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Generic Utility (Loki)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
template <MUINT32 v>
struct Int2Type {
  enum { value = v };
};

template <MBOOL>
struct CompileTimeError;
template <>
struct CompileTimeError<MTRUE> {};

#define STATIC_CHECK(expr, msg)                  \
  {                                              \
    CompileTimeError<((expr) != 0)> ERROR_##msg; \
    (void)ERROR_##msg;                           \
  }

template <MBOOL _isT1, typename T1, typename T2>
struct SelectType {
  typedef T1 Type;
};
template <typename T1, typename T2>
struct SelectType<MFALSE, T1, T2> {
  typedef T2 Type;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Enumeration for subitems supported by a given Featrue ID.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
template <MUINT32 _fid>
struct Fid2Type {};

#undef FTYPE_ENUM
#undef FID_TO_TYPE_ENUM
#define FTYPE_ENUM(_enums...) _enums
#define FID_TO_TYPE_ENUM(_fid, _enums)                  \
  typedef enum { _enums, OVER_NUM_OF_##_fid } _fid##_T; \
  template <>                                           \
  struct Fid2Type<_fid> {                               \
    typedef _fid##_T Type;                              \
    typedef FidInfo<_fid> Info;                         \
    enum {                                              \
      Num = (OVER_NUM_OF_##_fid - 1),                   \
    };                                                  \
  };                                                    \
  typedef _fid##_T

};  //  namespace NSFeature

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_FEATURE_CAMERA_FEATURE_UTILITY_H_
