// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_SERVICE_
#define SHILL_VPN_SERVICE_

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/service.h"

namespace shill {

class KeyValueStore;
class VPNDriver;

class VPNService : public Service {
 public:
  VPNService(ControlInterface *control,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager,
             VPNDriver *driver);  // Takes ownership of |driver|.
  virtual ~VPNService();

  // Inherited from Service.
  virtual bool TechnologyIs(const Technology::Identifier type) const;
  virtual void Connect(Error *error);
  virtual void Disconnect(Error *error);
  virtual std::string GetStorageIdentifier() const;
  virtual bool Load(StoreInterface *storage);
  virtual bool Save(StoreInterface *storage);
  virtual bool Unload();

  virtual void InitDriverPropertyStore();

  VPNDriver *driver() const { return driver_.get(); }

  static std::string CreateStorageIdentifier(const KeyValueStore &args,
                                             Error *error);
  void set_storage_id(const std::string &id) { storage_id_ = id; }

 private:
  FRIEND_TEST(VPNServiceTest, GetDeviceRpcId);

  virtual std::string GetDeviceRpcId(Error *error);

  std::string storage_id_;
  scoped_ptr<VPNDriver> driver_;

  // Provided only for compatibility.  crosbug.com/29286
  std::string vpn_domain_;

  DISALLOW_COPY_AND_ASSIGN(VPNService);
};

}  // namespace shill

#endif  // SHILL_VPN_SERVICE_
