// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_ITERATOR_H_
#define SHILL_PROPERTY_ITERATOR_H_

#include <map>
#include <memory>
#include <string>

#include "shill/accessor_interface.h"
#include "shill/error.h"

namespace shill {

// An iterator wrapper class to hide the details of what kind of data structure
// we're using to store key/value pairs for properties. It is intended for use
// with PropertyStore and always advances to the next readable property.
template <class V>
class ReadablePropertyConstIterator {
 public:
  ~ReadablePropertyConstIterator() = default;

  bool AtEnd() const { return it_ == collection_.end(); }

  void Advance() {
    if (!AtEnd()) {
      do {
        ++it_;
      } while (MustAdvance());
    }
  }

  const std::string& Key() const { return it_->first; }

  const V& value() const { return value_; }

 private:
  friend class PropertyStore;

  explicit ReadablePropertyConstIterator(const AccessorMap<V>& collection)
      : collection_(collection), it_(collection_.begin()), value_() {
    if (MustAdvance()) {
      Advance();
    }
  }

  bool MustAdvance() { return !AtEnd() && !RetrieveCurrentValue(); }

  bool RetrieveCurrentValue() {
    Error error;
    value_ = it_->second->Get(&error);
    return error.IsSuccess();
  }

  const AccessorMap<V>& collection_;
  typename AccessorMap<V>::const_iterator it_;
  V value_;
};

}  // namespace shill

#endif  // SHILL_PROPERTY_ITERATOR_H_
