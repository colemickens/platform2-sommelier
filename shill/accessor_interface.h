// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ACCESSOR_INTERFACE_
#define SHILL_ACCESSOR_INTERFACE_

#include <map>
#include <string>
#include <tr1/memory>
#include <utility>
#include <vector>

#include <base/basictypes.h>

namespace shill {

class Error;

// A templated abstract base class for objects that can be used to access
// properties stored in objects that are meant to be made available over RPC.
// The intended usage is that an object stores a maps of strings to
// AccessorInterfaces of the appropriate type, and then uses
// map[name]->Get() and map[name]->Set(value) to get and set the properties.
template <class T>
class AccessorInterface {
 public:
  AccessorInterface() {}
  virtual ~AccessorInterface() {}

  // Provides read-only access.
  virtual const T &Get() = 0;
  // Attempts to set the wrapped value. Sets |error| on failure.
  virtual void Set(const T &value, Error *error) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessorInterface);
};

// This class stores two dissimilar named properties, one named string
// property and one named unsigned integer property.
class StrIntPair {
 public:
  StrIntPair() {}
  StrIntPair(std::pair<std::string, std::string> string_prop,
             std::pair<std::string, uint32> uint_prop) {
    string_property_[string_prop.first] = string_prop.second;
    uint_property_[uint_prop.first] = uint_prop.second;
  }
  ~StrIntPair() {}

  const std::map<std::string, std::string> &string_property() const {
    return string_property_;
  }
  const std::map<std::string, uint32> &uint_property() const {
    return uint_property_;
  }

 private:
  // These are stored as maps because it's way easier to coerce them to
  // the right DBus types than if they were pairs.
  std::map<std::string, std::string> string_property_;
  std::map<std::string, uint32> uint_property_;
};

typedef std::vector<uint8_t> ByteArray;
typedef std::vector<ByteArray> ByteArrays;
typedef std::vector<std::string> Strings;
typedef std::map<std::string, std::string> Stringmap;
typedef std::vector<Stringmap> Stringmaps;

// Using a smart pointer here allows pointers to classes derived from
// AccessorInterface<> to be stored in maps and other STL container types.
typedef std::tr1::shared_ptr<AccessorInterface<bool> > BoolAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<int16> > Int16Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<int32> > Int32Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<std::string> > StringAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<Stringmap> > StringmapAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<Stringmaps> > StringmapsAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<Strings> > StringsAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<StrIntPair> > StrIntPairAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<uint8> > Uint8Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<uint16> > Uint16Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<uint32> > Uint32Accessor;

}  // namespace shill

#endif  // SHILL_ACCESSOR_INTERFACE_
