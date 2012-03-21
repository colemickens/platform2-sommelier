// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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

#include "shill/key_value_store.h"

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

  // Reset the property to its default value. Sets |error| on failure.
  virtual void Clear(Error *error) = 0;
  // Provides read-only access. Sets |error| on failure.
  virtual T Get(Error *error) = 0;
  // Attempts to set the wrapped value. Sets |error| on failure.
  virtual void Set(const T &value, Error *error) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessorInterface);
};

typedef std::vector<uint8_t> ByteArray;
typedef std::vector<ByteArray> ByteArrays;
// Note that while the RpcIdentifiers type has the same concrete
// representation as the Strings type, it may be serialized
// differently. Accordingly, PropertyStore tracks RpcIdentifiers
// separately from Strings. We create a separate typedef here, to make
// the PropertyStore-related code read more simply.
typedef std::vector<std::string> RpcIdentifiers;
typedef std::vector<std::string> Strings;
typedef std::map<std::string, std::string> Stringmap;
typedef std::vector<Stringmap> Stringmaps;

// Using a smart pointer here allows pointers to classes derived from
// AccessorInterface<> to be stored in maps and other STL container types.
typedef std::tr1::shared_ptr<AccessorInterface<bool> > BoolAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<int16> > Int16Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<int32> > Int32Accessor;
// See comment above RpcIdentifiers typedef, for the reason why the
// RpcIdentifiersAccessor exists (even though it has the same
// underlying type as StringsAccessor).
typedef std::tr1::shared_ptr<
  AccessorInterface<std::vector<std::string> > >RpcIdentifiersAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<std::string> > StringAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<Stringmap> > StringmapAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<Stringmaps> > StringmapsAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<Strings> > StringsAccessor;
typedef std::tr1::shared_ptr<
  AccessorInterface<KeyValueStore> > KeyValueStoreAccessor;
typedef std::tr1::shared_ptr<AccessorInterface<uint8> > Uint8Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<uint16> > Uint16Accessor;
typedef std::tr1::shared_ptr<AccessorInterface<uint32> > Uint32Accessor;

}  // namespace shill

#endif  // SHILL_ACCESSOR_INTERFACE_
