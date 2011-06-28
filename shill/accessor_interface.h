// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ACCESSOR_INTERFACE_
#define SHILL_ACCESSOR_INTERFACE_

#include <map>
#include <string>
#include <tr1/memory>
#include <vector>

#include <base/basictypes.h>

namespace shill {

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
  // Attempts to set the wrapped value.  Returns true upon success.
  virtual bool Set(const T &value) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessorInterface);
};

typedef std::vector<std::string> Strings;
typedef std::map<std::string, std::string> Stringmap;

// Using a smart pointer here allows pointers to classes derived from
// AccessorInterface<> to be stored in maps and other STL container types.
typedef std::tr1::shared_ptr<AccessorInterface<bool> > BoolAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<int16> > Int16Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<int32> > Int32Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<std::string> > StringAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<Stringmap> > StringmapAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<Strings> > StringsAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<uint8> > Uint8Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<uint16> > Uint16Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<uint32> > Uint32Accessor;

}  // namespace shill

#endif  // SHILL_ACCESSOR_INTERFACE_
