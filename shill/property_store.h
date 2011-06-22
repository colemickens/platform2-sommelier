// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_STORE_
#define SHILL_PROPERTY_STORE_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>

#include "shill/accessor_interface.h"

namespace shill {

class Error;

class PropertyStore {
 public:
  virtual ~PropertyStore();

  virtual bool Contains(const std::string& property);

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
  PropertyStore();

  void RegisterBool(const std::string &name, bool *prop);
  void RegisterConstBool(const std::string &name, const bool *prop);
  void RegisterInt16(const std::string &name, int16 *prop);
  void RegisterConstInt16(const std::string &name, const int16 *prop);
  void RegisterInt32(const std::string &name, int32 *prop);
  void RegisterConstInt32(const std::string &name, const int32 *prop);
  void RegisterString(const std::string &name, std::string *prop);
  void RegisterConstString(const std::string &name, const std::string *prop);
  void RegisterStringmap(const std::string &name,
                         std::map<std::string, std::string> *prop);
  void RegisterConstStringmap(const std::string &name,
                              const std::map<std::string, std::string> *prop);
  void RegisterConstUint8(const std::string &name, const uint8 *prop);
  void RegisterUint16(const std::string &name, uint16 *prop);
  void RegisterConstUint16(const std::string &name, const uint16 *prop);

  // These are std::maps instead of something cooler because the common
  // operation is iterating through them and returning all properties.
  std::map<std::string, BoolAccessor> bool_properties_;
  std::map<std::string, Int16Accessor> int16_properties_;
  std::map<std::string, Int32Accessor> int32_properties_;
  std::map<std::string, StringAccessor> string_properties_;
  std::map<std::string, StringmapAccessor> stringmap_properties_;
  std::map<std::string, StringsAccessor> strings_properties_;
  std::map<std::string, Uint8Accessor> uint8_properties_;
  std::map<std::string, Uint16Accessor> uint16_properties_;
  std::map<std::string, Uint32Accessor> uint32_properties_;

 private:
  bool ReturnError(const std::string& name, Error *error);

  DISALLOW_COPY_AND_ASSIGN(PropertyStore);
};

}  // namespace shill

#endif  // SHILL_PROPERTY_STORE_
