// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_L2TP_IPSEC_DRIVER_
#define SHILL_L2TP_IPSEC_DRIVER_

#include "shill/vpn_driver.h"

namespace shill {

class L2TPIPSecDriver : public VPNDriver {
 public:
  L2TPIPSecDriver();
  virtual ~L2TPIPSecDriver();

  // Inherited from VPNDriver.
  virtual bool ClaimInterface(const std::string &link_name,
                              int interface_index);
  virtual void Connect(const VPNServiceRefPtr &service, Error *error);
  virtual void Disconnect();
  virtual bool Load(StoreInterface *storage, const std::string &storage_id);
  virtual bool Save(StoreInterface *storage, const std::string &storage_id);
  virtual void InitPropertyStore(PropertyStore *store);
  virtual std::string GetProviderType() const;

 private:
  friend class L2TPIPSecDriverTest;

  DISALLOW_COPY_AND_ASSIGN(L2TPIPSecDriver);
};

}  // namespace shill

#endif  // SHILL_L2TP_IPSEC_DRIVER_
