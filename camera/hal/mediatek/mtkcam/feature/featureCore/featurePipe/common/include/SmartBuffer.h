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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_SMARTBUFFER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_SMARTBUFFER_H_

#include <memory>

#define COMPARE(op)                                                        \
  inline bool operator op(const sb<T>& o) const { return mPtr op o.mPtr; } \
  inline bool operator op(const T* o) const { return mPtr op o; }          \
  template <typename U>                                                    \
  inline bool operator op(const sb<U>& o) const {                          \
    return mPtr op o.mPtr;                                                 \
  }                                                                        \
  template <typename U>                                                    \
  inline bool operator op(const U* o) const {                              \
    return mPtr op o;                                                      \
  }

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename T>
class sb {
 public:
  sb() : mPtr(nullptr) {}

  explicit sb(T* other);
  sb(const sb<T>& other);
  explicit sb(const std::shared_ptr<T>& other);
  ~sb();

  sb& operator=(T* other);
  sb& operator=(const sb<T>& other);
  sb& operator=(const std::shared_ptr<T>& other);

  inline T& operator*() const { return *mPtr; }
  inline T* operator->() const { return mPtr.get(); }
  inline T* get() const { return mPtr.get(); }

  COMPARE(==);
  COMPARE(!=);

  inline bool operator>(const sb<T>& o) const { return mPtr > o.mPtr; }
  inline bool operator>(const T* o) const { return mPtr > o; }
  template <typename U>
  inline bool operator>(const sb<U>& o) const {
    return mPtr > o.mPtr;
  }
  template <typename U>
  inline bool operator>(const U* o) const {
    return mPtr > o;
  }

  inline bool operator<(const sb<T>& o) const { return mPtr < o.mPtr; }
  inline bool operator<(const T* o) const { return mPtr < o; }
  template <typename U>
  inline bool operator<(const sb<U>& o) const {
    return mPtr < o.mPtr;
  }
  template <typename U>
  inline bool operator<(const U* o) const {
    return mPtr < o;
  }

  COMPARE(<=);
  COMPARE(>=);

 private:
  void inc(const std::shared_ptr<T>& ptr);
  void dec(const std::shared_ptr<T>& ptr);
  std::shared_ptr<T> mPtr;
};

template <typename T>
void sb<T>::inc(const std::shared_ptr<T>& ptr) {
  if (ptr != nullptr) {
    ptr->incSbCount();
  }
}

template <typename T>
void sb<T>::dec(const std::shared_ptr<T>& ptr) {
  if (ptr != nullptr) {
    ptr->decSbCount();
  }
}

template <typename T>
sb<T>::sb(T* other) : mPtr(other) {
  inc(other);
}

template <typename T>
sb<T>::sb(const sb<T>& other) : mPtr(other.mPtr) {
  inc(mPtr);
}

template <typename T>
sb<T>::sb(const std::shared_ptr<T>& other) : mPtr(other) {
  inc(mPtr);
}

template <typename T>
sb<T>::~sb() {
  dec(mPtr);
}

template <typename T>
sb<T>& sb<T>::operator=(const sb<T>& other) {
  std::shared_ptr<T> otherPtr(other.mPtr);
  inc(otherPtr);
  dec(mPtr);
  mPtr = otherPtr;
  return *this;
}

template <typename T>
sb<T>& sb<T>::operator=(const std::shared_ptr<T>& other) {
  inc(other);
  dec(mPtr);
  mPtr = other;
  return *this;
}

template <typename T>
sb<T>& sb<T>::operator=(T* other) {
  inc(other);
  dec(mPtr);
  mPtr = other;
  return *this;
}

#undef COMPARE

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_SMARTBUFFER_H_
