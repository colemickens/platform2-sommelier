// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DLCSERVICE_DLC_SERVICE_DBUS_ADAPTOR_H_
#define DLCSERVICE_DLC_SERVICE_DBUS_ADAPTOR_H_

#include <string>

#include "dlcservice/dbus_adaptors/org.chromium.DlcServiceInterface.h"

namespace dlcservice {

// DlcServiceDBusAdaptor is a D-Bus adaptor that manages life-cycles of DLCs
// (Downloadable Content) and provides an API for the rest of the system to
// install/uninstall DLCs.
class DlcServiceDBusAdaptor
    : public org::chromium::DlcServiceInterfaceInterface,
      public org::chromium::DlcServiceInterfaceAdaptor {
 public:
  DlcServiceDBusAdaptor();
  ~DlcServiceDBusAdaptor() override = default;
  // org::chromium::DlServiceInterfaceInterface overrides:
  bool Install(brillo::ErrorPtr* err,
               const std::string& id_in,
               std::string* mount_point_out) override;
  bool Uninstall(brillo::ErrorPtr* err, const std::string& id_in) override;
};

}  // namespace dlcservice

#endif  // DLCSERVICE_DLC_SERVICE_DBUS_ADAPTOR_H_
