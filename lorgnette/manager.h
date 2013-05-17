// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LORGNETTE_MANAGER_H_
#define LORGNETTE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/callback.h>
#include <dbus-c++/dbus.h>

#include "lorgnette/dbus_bindings/lorgnette-manager.h"

namespace lorgnette {

class Minijail;

class Manager
    : public org::chromium::lorgnette::Manager_adaptor,
      public DBus::ObjectAdaptor,
      public DBus::IntrospectableAdaptor {
 public:
  Manager(DBus::Connection *connection);
  virtual ~Manager();

  // Implementation of org::chromium::lorgnette::Manager_adaptor.
  virtual std::map<std::string, std::map<std::string, std::string>>
      ListScanners(::DBus::Error &error);
  virtual void ScanImage(
      const std::string &device_name,
      const ::DBus::FileDescriptor &outfd,
      const std::map<std::string, ::DBus::Variant> &scan_properties,
      ::DBus::Error &error);

 private:
  static const char kDBusErrorName[];
  static const char kObjectPath[];
  static const char kScanConverterPath[];
  static const char kScanImageFormattedDeviceListCmd[];
  static const char kScanImagePath[];
  static const char kScannerPropertyManufacturer[];
  static const char kScannerPropertyModel[];
  static const char kScannerPropertyType[];
  static const char kScanPropertyMode[];
  static const char kScanPropertyModeColor[];
  static const char kScanPropertyModeGray[];
  static const char kScanPropertyModeLineart[];
  static const char kScanPropertyResolution[];
  static const int kTimeoutAfterKillSeconds;

  static void SetError(const std::string &method,
                       const std::string &message,
                       ::DBus::Error *error);

  Minijail *minijail_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace lorgnette

#endif  // LORGNETTE_MANAGER_H_
