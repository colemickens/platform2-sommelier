// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_KEY_VALUE_STORE_
#define SHILL_KEY_VALUE_STORE_

#include <map>
#include <string>

#include <base/basictypes.h>

namespace shill {

class KeyValueStore {
  // A simple store for key-value pairs, which supports (a limited set of)
  // heterogenous value types.
  //
  // Compare to PropertyStore, which enables a class to (selectively)
  // expose its instance members as properties accessible via
  // RPC. (RPC support for ProperyStore is implemented in a
  // protocol-specific adaptor. e.g. dbus_adpator.)
  //
  // Implemented separately from PropertyStore, to avoid complicating
  // the PropertyStore interface. In particular, objects implementing the
  // PropertyStore interface always provide the storage themselves. In
  // contrast, users of KeyValueStore expect KeyValueStore to provide
  // storage.
 public:
  KeyValueStore();

  bool ContainsBool(const std::string &name) const;
  bool ContainsString(const std::string &name) const;

  bool GetBool(const std::string &name) const;
  const std::string &GetString(const std::string &name) const;

  void SetBool(const std::string &name, bool value);
  void SetString(const std::string& name,
                         const std::string& value);

 private:
  std::map<std::string, bool> bool_properties_;
  std::map<std::string, std::string> string_properties_;

  DISALLOW_COPY_AND_ASSIGN(KeyValueStore);
};

}  // namespace shill

#endif  // SHILL_KEY_VALUE_STORE_
