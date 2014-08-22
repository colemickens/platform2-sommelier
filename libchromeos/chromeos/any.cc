// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/any.h>

#include <algorithm>

namespace chromeos {

Any::Any(const Any& rhs) : data_buffer_(rhs.data_buffer_) {
}

// NOLINTNEXTLINE(build/c++11)
Any::Any(Any&& rhs) : data_buffer_(std::move(rhs.data_buffer_)) {
}

Any::~Any() {
}

Any& Any::operator=(const Any& rhs) {
  data_buffer_ = rhs.data_buffer_;
  return *this;
}

// NOLINTNEXTLINE(build/c++11)
Any& Any::operator=(Any&& rhs) {
  data_buffer_ = std::move(rhs.data_buffer_);
  return *this;
}

const std::type_info& Any::GetType() const {
  if (!IsEmpty())
    return data_buffer_.GetDataPtr()->GetType();

  struct NullType {};  // Special helper type representing an empty variant.
  return typeid(NullType);
}

void Any::Swap(Any& other) {
  std::swap(data_buffer_, other.data_buffer_);
}

bool Any::IsEmpty() const {
  return data_buffer_.IsEmpty();
}

void Any::Clear() {
  data_buffer_.Clear();
}

bool Any::IsConvertibleToInteger() const {
  return !IsEmpty() && data_buffer_.GetDataPtr()->IsConvertibleToInteger();
}

intmax_t Any::GetAsInteger() const {
  CHECK(!IsEmpty()) << "Must not be called on an empty Any";
  return data_buffer_.GetDataPtr()->GetAsInteger();
}

}  // namespace chromeos
