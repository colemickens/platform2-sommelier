// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_DRIVER_
#define SHILL_VPN_DRIVER_

#include <string>

#include <base/basictypes.h>

#include "shill/refptr_types.h"

namespace shill {

class Error;
class PropertyStore;
class StoreInterface;

class VPNDriver {
 public:
  virtual ~VPNDriver() {}

  virtual bool ClaimInterface(const std::string &link_name,
                              int interface_index) = 0;
  virtual void Connect(const VPNServiceRefPtr &service, Error *error) = 0;
  virtual void Disconnect() = 0;
  virtual bool Load(StoreInterface *storage, const std::string &storage_id) = 0;
  virtual bool Save(StoreInterface *storage, const std::string &storage_id) = 0;
  virtual void InitPropertyStore(PropertyStore *store) = 0;
};

}  // namespace shill

#endif  // SHILL_VPN_DRIVER_
