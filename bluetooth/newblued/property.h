// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_PROPERTY_H_
#define BLUETOOTH_NEWBLUED_PROPERTY_H_

#include <utility>

namespace bluetooth {

// A generic wrapper around a property of an interface without involving D-Bus
// operations.
template <typename T>
class Property {
 public:
  Property() : value_(), updated_(false) {}

  explicit Property(const T& value) : value_(value), updated_(false) {}
  explicit Property(T&& value) : value_(std::move(value)), updated_(false) {}

  // Setters of the property value.
  void SetValue(const T& new_value) {
    if (value_ != new_value) {
      value_ = new_value;
      updated_ = true;
    }
  }
  void SetValue(T&& new_value) {
    if (value_ != new_value) {
      value_ = std::move(new_value);
      updated_ = true;
    }
  }

  void ClearUpdated() { updated_ = false; }

  const T& value() const { return value_; }
  bool updated() const { return updated_; }

 private:
  // Value of the property.
  T value_;
  // Whether the value is changed.
  bool updated_;

  DISALLOW_COPY_AND_ASSIGN(Property);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_PROPERTY_H_
