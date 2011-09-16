// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_ADAPTOR_H_
#define SHILL_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "shill/accessor_interface.h"

namespace shill {

#define SHILL_INTERFACE "org.chromium.flimflam"
#define SHILL_PATH "/org/chromium/flimflam"

class PropertyStore;

// Superclass for all DBus-backed Adaptor objects
class DBusAdaptor : public DBus::ObjectAdaptor,
                    public DBus::IntrospectableAdaptor {
 public:
  DBusAdaptor(DBus::Connection* conn, const std::string &object_path);
  virtual ~DBusAdaptor();

  static bool DispatchOnType(PropertyStore *store,
                             const std::string &name,
                             const ::DBus::Variant &value,
                             ::DBus::Error *error);
  static bool GetProperties(const PropertyStore &store,
                            std::map<std::string, ::DBus::Variant> *out,
                            ::DBus::Error *error);

  static ::DBus::Variant BoolToVariant(bool value);
  static ::DBus::Variant ByteToVariant(uint8 value);
  static ::DBus::Variant Int16ToVariant(int16 value);
  static ::DBus::Variant Int32ToVariant(int32 value);
  static ::DBus::Variant PathToVariant(const ::DBus::Path &value);
  static ::DBus::Variant PathArrayToVariant(
      const std::vector< ::DBus::Path> &value);
  static ::DBus::Variant StringToVariant(const std::string &value);
  static ::DBus::Variant StringmapToVariant(const Stringmap &value);
  static ::DBus::Variant StringmapsToVariant(const Stringmaps &value);
  static ::DBus::Variant StringsToVariant(const Strings &value);
  static ::DBus::Variant StrIntPairToVariant(const StrIntPair &value);
  static ::DBus::Variant Uint16ToVariant(uint16 value);
  static ::DBus::Variant Uint32ToVariant(uint32 value);

  static bool IsBool(::DBus::Signature signature);
  static bool IsByte(::DBus::Signature signature);
  static bool IsInt16(::DBus::Signature signature);
  static bool IsInt32(::DBus::Signature signature);
  static bool IsPath(::DBus::Signature signature);
  static bool IsPathArray(::DBus::Signature signature);
  static bool IsString(::DBus::Signature signature);
  static bool IsStringmap(::DBus::Signature signature);
  static bool IsStringmaps(::DBus::Signature signature);
  static bool IsStrings(::DBus::Signature signature);
  static bool IsUint16(::DBus::Signature signature);
  static bool IsUint32(::DBus::Signature signature);

 private:
  static const char kPathArraySig[];
  static const char kStringmapSig[];
  static const char kStringmapsSig[];
  static const char kStringsSig[];
  DISALLOW_COPY_AND_ASSIGN(DBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_DBUS_ADAPTOR_H_
