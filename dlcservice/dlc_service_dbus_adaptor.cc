// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service_dbus_adaptor.h"

namespace dlcservice {

DlcServiceDBusAdaptor::DlcServiceDBusAdaptor()
    : org::chromium::DlcServiceInterfaceAdaptor(this) {}

bool DlcServiceDBusAdaptor::Install(brillo::ErrorPtr* err,
                                    const std::string& in_id,
                                    std::string* mount_point_out) {
  // TODO(xiaochu): implement this.
  *mount_point_out = "";
  return true;
}

bool DlcServiceDBusAdaptor::Uninstall(brillo::ErrorPtr* err,
                                      const std::string& id_in) {
  // TODO(xiaochu): implement this.
  return true;
}

}  // namespace dlcservice
