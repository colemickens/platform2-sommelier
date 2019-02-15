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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_REFLECTION_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_REFLECTION_H_

#include <iostream>

/******************************************************************************
 * Variadic Macro
 ******************************************************************************/
#define _GET_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, \
                     _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24,  \
                     _25, _26, _27, _28, N, ...)                             \
  N
#define COUNT_VARARGS(...)                                                  \
  _GET_NTH_ARG(__VA_ARGS__, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
               16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define _FOREACH_0(_call, i, ...)
#define _FOREACH_1(_call, i, x) _call(i, x)
#define _FOREACH_2(_call, i, x, ...) \
  _call(i, x) _FOREACH_1(_call, i + 1, __VA_ARGS__)
#define _FOREACH_3(_call, i, x, ...) \
  _call(i, x) _FOREACH_2(_call, i + 1, __VA_ARGS__)
#define _FOREACH_4(_call, i, x, ...) \
  _call(i, x) _FOREACH_3(_call, i + 1, __VA_ARGS__)
#define _FOREACH_5(_call, i, x, ...) \
  _call(i, x) _FOREACH_4(_call, i + 1, __VA_ARGS__)
#define _FOREACH_6(_call, i, x, ...) \
  _call(i, x) _FOREACH_5(_call, i + 1, __VA_ARGS__)
#define _FOREACH_7(_call, i, x, ...) \
  _call(i, x) _FOREACH_6(_call, i + 1, __VA_ARGS__)
#define _FOREACH_8(_call, i, x, ...) \
  _call(i, x) _FOREACH_7(_call, i + 1, __VA_ARGS__)
#define _FOREACH_9(_call, i, x, ...) \
  _call(i, x) _FOREACH_8(_call, i + 1, __VA_ARGS__)
#define _FOREACH_10(_call, i, x, ...) \
  _call(i, x) _FOREACH_9(_call, i + 1, __VA_ARGS__)
#define _FOREACH_11(_call, i, x, ...) \
  _call(i, x) _FOREACH_10(_call, i + 1, __VA_ARGS__)
#define _FOREACH_12(_call, i, x, ...) \
  _call(i, x) _FOREACH_11(_call, i + 1, __VA_ARGS__)
#define _FOREACH_13(_call, i, x, ...) \
  _call(i, x) _FOREACH_12(_call, i + 1, __VA_ARGS__)
#define _FOREACH_14(_call, i, x, ...) \
  _call(i, x) _FOREACH_13(_call, i + 1, __VA_ARGS__)
#define _FOREACH_15(_call, i, x, ...) \
  _call(i, x) _FOREACH_14(_call, i + 1, __VA_ARGS__)
#define _FOREACH_16(_call, i, x, ...) \
  _call(i, x) _FOREACH_15(_call, i + 1, __VA_ARGS__)
#define _FOREACH_17(_call, i, x, ...) \
  _call(i, x) _FOREACH_16(_call, i + 1, __VA_ARGS__)
#define _FOREACH_18(_call, i, x, ...) \
  _call(i, x) _FOREACH_17(_call, i + 1, __VA_ARGS__)
#define _FOREACH_19(_call, i, x, ...) \
  _call(i, x) _FOREACH_18(_call, i + 1, __VA_ARGS__)
#define _FOREACH_20(_call, i, x, ...) \
  _call(i, x) _FOREACH_19(_call, i + 1, __VA_ARGS__)
#define _FOREACH_21(_call, i, x, ...) \
  _call(i, x) _FOREACH_20(_call, i + 1, __VA_ARGS__)
#define _FOREACH_22(_call, i, x, ...) \
  _call(i, x) _FOREACH_21(_call, i + 1, __VA_ARGS__)
#define _FOREACH_23(_call, i, x, ...) \
  _call(i, x) _FOREACH_22(_call, i + 1, __VA_ARGS__)
#define _FOREACH_24(_call, i, x, ...) \
  _call(i, x) _FOREACH_23(_call, i + 1, __VA_ARGS__)
#define _FOREACH_25(_call, i, x, ...) \
  _call(i, x) _FOREACH_24(_call, i + 1, __VA_ARGS__)
#define _FOREACH_26(_call, i, x, ...) \
  _call(i, x) _FOREACH_25(_call, i + 1, __VA_ARGS__)
#define _FOREACH_27(_call, i, x, ...) \
  _call(i, x) _FOREACH_26(_call, i + 1, __VA_ARGS__)
#define _FOREACH_28(_call, i, x, ...) \
  _call(i, x) _FOREACH_27(_call, i + 1, __VA_ARGS__)

#define FOR_EACH(_macro, ...)                                                  \
  _GET_NTH_ARG(__VA_ARGS__, _FOREACH_28, _FOREACH_27, _FOREACH_26,             \
               _FOREACH_25, _FOREACH_24, _FOREACH_23, _FOREACH_22,             \
               _FOREACH_21, _FOREACH_20, _FOREACH_19, _FOREACH_18,             \
               _FOREACH_17, _FOREACH_16, _FOREACH_15, _FOREACH_14,             \
               _FOREACH_13, _FOREACH_12, _FOREACH_11, _FOREACH_10, _FOREACH_9, \
               _FOREACH_8, _FOREACH_7, _FOREACH_6, _FOREACH_5, _FOREACH_4,     \
               _FOREACH_3, _FOREACH_2, _FOREACH_1)                             \
  (_macro, 0, ##__VA_ARGS__);

/******************************************************************************
 *
 ******************************************************************************/
#define _STR(s) #s
#define STRINGIZE(s) _STR(s)
#define REM(...) __VA_ARGS__
#define EAT(...)

#define TYPEOF(x) DETAIL_TYPEOF(DETAIL_TYPEOF_PROBE x, )
#define DETAIL_TYPEOF(...) DETAIL_TYPEOF_HEAD(__VA_ARGS__)
#define DETAIL_TYPEOF_HEAD(x, ...) REM x
#define DETAIL_TYPEOF_PROBE(...) (__VA_ARGS__),
#define STRIP(x) EAT x
#define PAIR(x) REM x
#define FIELDS(...)                                       \
  static const int fields_n = COUNT_VARARGS(__VA_ARGS__); \
  friend struct Reflector;                                \
  template <int N, class T>                               \
  struct Field {};                                        \
  FOR_EACH(FIELD_EACH, __VA_ARGS__)

#define FIELD_EACH(i, x)                               \
  PAIR(x);                                             \
  template <class T>                                   \
  struct Field<i, T> {                                 \
    T& t;                                              \
    explicit Field(T& t) : t(t) {}                     \
    TYPEOF(x) get() { return t.STRIP(x); }             \
    const char* name() { return STRINGIZE(STRIP(x)); } \
  };

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_REFLECTION_H_
