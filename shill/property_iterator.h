// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_ITERATOR_
#define SHILL_PROPERTY_ITERATOR_

#include <map>
#include <string>

#include "shill/accessor_interface.h"
#include "shill/error.h"

class Error;

namespace shill {

// An iterator wrapper class to hide the details of what kind of data structure
// we're using to store key/value pairs for properties.
// Intended for use with PropertyStore.

// TODO(gauravsh): Consider getting rid of PropertyConstIterator, and just
// keeping ReadablePropertyConstIterator since it doesn't look like the caller
// is ever interested in anything but readable properties.
template <class V>
class PropertyConstIterator {
 public:
  virtual ~PropertyConstIterator() {}

  virtual void Advance() {
    if (!AtEnd())
      ++it_;
  }

  bool AtEnd() { return it_ == collection_.end(); }

  const std::string &Key() const { return it_->first; }

  V Value(Error *error) const { return it_->second->Get(error); }


 protected:
  typedef std::tr1::shared_ptr<AccessorInterface<V> > VAccessorPtr;

  explicit PropertyConstIterator(
      const typename std::map<std::string, VAccessorPtr> &collection)
      : collection_(collection),
        it_(collection_.begin()) {
  }

 private:
  friend class PropertyStore;

  const typename std::map<std::string, VAccessorPtr> &collection_;
  typename std::map<std::string, VAccessorPtr>::const_iterator it_;
};

// A version of the iterator that always advances to the next readable
// property.
template <class V>
class ReadablePropertyConstIterator : public PropertyConstIterator<V> {
 public:
  virtual ~ReadablePropertyConstIterator() {}

  virtual void Advance() {
    Error error;
    while (!PropertyConstIterator<V>::AtEnd()) {
      PropertyConstIterator<V>::Advance();
      if (PropertyConstIterator<V>::AtEnd())
        return;
      error.Reset();
      PropertyConstIterator<V>::Value(&error);
      if (error.IsSuccess())
        break;
    }
  }

 private:
  friend class PropertyStore;

  typedef std::tr1::shared_ptr<AccessorInterface<V> > VAccessorPtr;

  explicit ReadablePropertyConstIterator(
      const typename std::map<std::string, VAccessorPtr> &collection)
      : PropertyConstIterator<V>(collection) {
    if (PropertyConstIterator<V>::AtEnd())
      return;
    Error error;
    PropertyConstIterator<V>::Value(&error);
    // Start at the next readable property (if any).
    if (!error.IsSuccess())
      Advance();
  }
};

}  // namespace shill

#endif  // SHILL_PROPERTY_ITERATOR_
