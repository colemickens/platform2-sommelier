// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_ITERATOR_
#define SHILL_PROPERTY_ITERATOR_

#include <map>
#include <string>

#include "shill/accessor_interface.h"

namespace shill {

// An iterator wrapper class to hide the details of what kind of data structure
// we're using to store key/value pairs for properties.
// Intended for use with PropertyStore.
template <class V>
class PropertyConstIterator {
 public:
  virtual ~PropertyConstIterator() {}

  void Advance() { ++it_; }

  bool AtEnd() { return it_ == collection_.end(); }

  const std::string &Key() const { return it_->first; }

  const V &Value() const { return it_->second->Get(); }

 private:
  friend class PropertyStore;

  typedef std::tr1::shared_ptr<AccessorInterface<V> > VAccessorPtr;

  explicit PropertyConstIterator(
      const typename std::map<std::string, VAccessorPtr> &collection)
      : collection_(collection),
        it_(collection_.begin()) {
  }

  const typename std::map<std::string, VAccessorPtr> &collection_;
  typename std::map<std::string, VAccessorPtr>::const_iterator it_;
};



}  // namespace shill

#endif  // SHILL_PROPERTY_ITERATOR_
