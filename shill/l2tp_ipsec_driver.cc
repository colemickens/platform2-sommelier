// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/l2tp_ipsec_driver.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

using std::string;

namespace shill {

L2TPIPSecDriver::L2TPIPSecDriver() {}

L2TPIPSecDriver::~L2TPIPSecDriver() {}

bool L2TPIPSecDriver::ClaimInterface(const string &link_name,
                                     int interface_index) {
  // TODO(petkov): crosbug.com/26843.
  NOTIMPLEMENTED();
  return false;
}

void L2TPIPSecDriver::Connect(const VPNServiceRefPtr &service, Error *error) {
  // TODO(petkov): crosbug.com/26843.
  NOTIMPLEMENTED();
}

void L2TPIPSecDriver::Disconnect() {
  // TODO(petkov): crosbug.com/29364.
  NOTIMPLEMENTED();
}

bool L2TPIPSecDriver::Load(StoreInterface *storage, const string &storage_id) {
  // TODO(petkov): crosbug.com/29362.
  NOTIMPLEMENTED();
  return false;
}

bool L2TPIPSecDriver::Save(StoreInterface *storage, const string &storage_id) {
  // TODO(petkov): crosbug.com/29362.
  NOTIMPLEMENTED();
  return false;
}

void L2TPIPSecDriver::InitPropertyStore(PropertyStore *store) {
  // TODO(petkov): crosbug.com/29362.
  NOTIMPLEMENTED();
}

string L2TPIPSecDriver::GetProviderType() const {
  return flimflam::kProviderL2tpIpsec;
}

}  // namespace shill
