// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_STORE_INTERFACE_
#define SHILL_PROPERTY_STORE_INTERFACE_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>

namespace shill {

class Error;

class PropertyStoreInterface {
 public:
  virtual ~PropertyStoreInterface();

  // Methods to allow the setting, by name, of properties stored in this object.
  // The property names are declared in chromeos/dbus/service_constants.h,
  // so that they may be shared with libcros.
  // Upon success, these methods return true and leave |error| untouched.
  // Upon failure, they return false and set |error| appropriately, if it
  // is non-NULL.
  virtual bool SetBoolProperty(const std::string& name,
                               bool value,
                               Error *error);

  virtual bool SetInt16Property(const std::string& name,
                                int16 value,
                                Error *error);

  virtual bool SetInt32Property(const std::string& name,
                                int32 value,
                                Error *error);

  virtual bool SetStringProperty(const std::string& name,
                                 const std::string& value,
                                 Error *error);

  virtual bool SetStringmapProperty(
      const std::string& name,
      const std::map<std::string, std::string>& values,
      Error *error);

  virtual bool SetStringsProperty(const std::string& name,
                                  const std::vector<std::string>& values,
                                  Error *error);

  virtual bool SetUint8Property(const std::string& name,
                                uint8 value,
                                Error *error);

  virtual bool SetUint16Property(const std::string& name,
                                 uint16 value,
                                 Error *error);

  virtual bool SetUint32Property(const std::string& name,
                                 uint32 value,
                                 Error *error);

 protected:
  PropertyStoreInterface();

 private:
  bool ReturnError(const std::string& name, Error *error);

  DISALLOW_COPY_AND_ASSIGN(PropertyStoreInterface);
};

}  // namespace shill

#endif  // SHILL_PROPERTY_STORE_INTERFACE_
